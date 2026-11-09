#!/usr/bin/env python3
"""Convert .ogg music to the .mus format streamed by source/Music.cpp.

Usage:  python3 tools/mkmusic.py <input.ogg|folder> [more inputs...] -o <output folder>
Needs ffmpeg in PATH. Copy the result to the SD card as /mcdsefiles/music/<name>.mus

.mus layout (little endian):
  bytes 0..511   header: "MUS1", u16 sampleRate, u16 blockBytes (512), u32 blockCount, u32 sampleCount, zero padding
  bytes 512..    blockCount IMA-ADPCM blocks of blockBytes each (WAV style: s16 predictor, u8 stepIndex, u8 0, then
                 4-bit samples, low nibble first). A block holds (blockBytes-4)*2+1 = 1017 samples.
Everything is mono, 8000 Hz (the Beta music files are already 8 kHz). Blocks start on SD sector boundaries.
Each track is loudness-normalised (average -24 dB, peaks <= -1 dB, at most +18 dB of gain) so they all play at a similar volume.
"""
import os, re, struct, subprocess, sys, tempfile

RATE = 8000
BLOCK = 512
TARGET_MEAN_DB = -24.0   # average level every track is brought to (the Beta tracks differ by up to 25 dB)
PEAK_LIMIT_DB = -1.0     # never push the peaks above this
MAX_GAIN_DB = 18.0       # do not boost a very quiet track more than this

def gain_db(src):
    """Gain (dB) that brings the track to TARGET_MEAN_DB without clipping, from ffmpeg's volumedetect."""
    r = subprocess.run(["ffmpeg", "-hide_banner", "-i", src, "-ac", "1", "-ar", str(RATE), "-af", "volumedetect", "-f", "null", "-"],
                       capture_output=True, text=True)
    mean = re.search(r"mean_volume: (-?[\d.]+) dB", r.stderr)
    peak = re.search(r"max_volume: (-?[\d.]+) dB", r.stderr)
    if not mean or not peak:
        return 0.0
    return max(0.0, min(TARGET_MEAN_DB - float(mean.group(1)), PEAK_LIMIT_DB - float(peak.group(1)), MAX_GAIN_DB))

def convert(src, dst):
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "t.wav")
        g = gain_db(src)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-ac", "1", "-ar", str(RATE), "-af", "volume=%.2fdB" % g,
                        "-c:a", "adpcm_ima_wav", "-block_size", str(BLOCK), wav], check=True)
        d = open(wav, "rb").read()
    # walk RIFF chunks to find "data"
    pos = 12
    data = None
    while pos + 8 <= len(d):
        cid = d[pos:pos + 4]
        size = struct.unpack_from("<I", d, pos + 4)[0]
        if cid == b"data":
            data = d[pos + 8: pos + 8 + size]
            break
        pos += 8 + size + (size & 1)
    if data is None:
        raise RuntimeError("no data chunk in " + src)
    nblocks = (len(data) + BLOCK - 1) // BLOCK
    data = data + b"\0" * (nblocks * BLOCK - len(data))
    spb = (BLOCK - 4) * 2 + 1
    header = b"MUS1" + struct.pack("<HHII", RATE, BLOCK, nblocks, nblocks * spb)
    header += b"\0" * (512 - len(header))
    with open(dst, "wb") as f:
        f.write(header)
        f.write(data)
    return nblocks * spb / RATE

def main():
    args = sys.argv[1:]
    if "-o" not in args:
        print(__doc__); sys.exit(1)
    i = args.index("-o")
    out = args[i + 1]
    ins = args[:i] + args[i + 2:]
    os.makedirs(out, exist_ok=True)
    files = []
    for p in ins:
        if os.path.isdir(p):
            files += [os.path.join(p, n) for n in sorted(os.listdir(p)) if n.lower().endswith(".ogg")]
        else:
            files.append(p)
    for src in files:
        name = os.path.splitext(os.path.basename(src))[0].lower() + ".mus"
        secs = convert(src, os.path.join(out, name))
        print("%-14s %6.1f s  %7d bytes" % (name, secs, os.path.getsize(os.path.join(out, name))))

if __name__ == "__main__":
    main()

#ifndef GOVERNOR_H
#define GOVERNOR_H

// Regulador de carga. El motor ya reparte la generación de chunks, la luz y las listas de dibujo con presupuestos de tiempo, pero siempre hace al
// menos un paso por cuadro, y los pasos no se pueden partir: varios pasos pesados seguidos (un bosque con árboles grandes y cuevas debajo, por
// ejemplo) mantienen los cuadros en 40 o 50 ms. El regulador mira cuánto tardó el cuadro anterior y, si se pasó, afloja el trabajo pesado:
// recorta los presupuestos y salta el trabajo pesado de uno o dos cuadros (nunca más de dos seguidos, para que el mundo siga cargando).
namespace Governor {
    void setEnabled(bool on);               // se apaga en la pantalla de carga: ahí lo que importa es terminar
    void onFrameEnd(unsigned int frameTicks);   // al terminar cada cuadro, con los ticks del temporizador (33514 por milisegundo)
    bool skipHeavy();                       // true: en este cuadro no se hace trabajo pesado de generación ni se reconstruyen listas de dibujo
    int budgetPercent();                    // 100 normal; 50 si el cuadro anterior pasó de 20 ms (se aplica a los presupuestos de Chunk.cpp)
}

#endif
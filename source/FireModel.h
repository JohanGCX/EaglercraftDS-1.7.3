#ifndef FIREMODEL_H
#define FIREMODEL_H

// Modelo del fuego de Beta 1.7.3: RenderBlocks.renderBlockFire traducido automáticamente del texto original (solo cambian
// las consultas al mundo y la salida de vértices). (var2, var3, var4) = celda; var6 = índice de textura de la primera capa (31).
// Los vértices salen en coordenadas absolutas de celda, con u y v en fracciones del atlas (0..1).
template <class Catch, class Normal, class Emit>
static void fireModel(int var2, int var3, int var4, int var6, Catch canCatch, Normal isNormal, Emit emit) {
		int var8 = (var6 & 15) << 4;
		int var9 = var6 & 240;
		double var10 = (double)((float)var8 / 256.0F);
		double var12 = (double)(((float)var8 + 15.99F) / 256.0F);
		double var14 = (double)((float)var9 / 256.0F);
		double var16 = (double)(((float)var9 + 15.99F) / 256.0F);
		float var18 = 1.4F;
		double var21;
		double var23;
		double var25;
		double var27;
		double var29;
		double var31;
		double var33;
		if(!isNormal(var2, var3 - 1, var4) && !canCatch(var2, var3 - 1, var4)) {
			float var37 = 0.2F;
			float var20 = 1.0F / 16.0F;
			if(((var2 + var3 + var4) & 1) == 1) {
				var10 = (double)((float)var8 / 256.0F);
				var12 = (double)(((float)var8 + 15.99F) / 256.0F);
				var14 = (double)((float)(var9 + 16) / 256.0F);
				var16 = (double)(((float)var9 + 15.99F + 16.0F) / 256.0F);
			}

			if(((var2 / 2 + var3 / 2 + var4 / 2) & 1) == 1) {
				var21 = var12;
				var12 = var10;
				var10 = var21;
			}

			if(canCatch(var2 - 1, var3, var4)) {
				emit((double)((float)var2 + var37), (double)((float)var3 + var18 + var20), (double)(var4 + 1), var12, var14);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1), var12, var16);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)((float)var2 + var37), (double)((float)var3 + var18 + var20), (double)(var4 + 0), var10, var14);
				emit((double)((float)var2 + var37), (double)((float)var3 + var18 + var20), (double)(var4 + 0), var10, var14);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1), var12, var16);
				emit((double)((float)var2 + var37), (double)((float)var3 + var18 + var20), (double)(var4 + 1), var12, var14);
			}

			if(canCatch(var2 + 1, var3, var4)) {
				emit((double)((float)(var2 + 1) - var37), (double)((float)var3 + var18 + var20), (double)(var4 + 0), var10, var14);
				emit((double)(var2 + 1 - 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)(var2 + 1 - 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1), var12, var16);
				emit((double)((float)(var2 + 1) - var37), (double)((float)var3 + var18 + var20), (double)(var4 + 1), var12, var14);
				emit((double)((float)(var2 + 1) - var37), (double)((float)var3 + var18 + var20), (double)(var4 + 1), var12, var14);
				emit((double)(var2 + 1 - 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1), var12, var16);
				emit((double)(var2 + 1 - 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)((float)(var2 + 1) - var37), (double)((float)var3 + var18 + var20), (double)(var4 + 0), var10, var14);
			}

			if(canCatch(var2, var3, var4 - 1)) {
				emit((double)(var2 + 0), (double)((float)var3 + var18 + var20), (double)((float)var4 + var37), var12, var14);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var12, var16);
				emit((double)(var2 + 1), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)(var2 + 1), (double)((float)var3 + var18 + var20), (double)((float)var4 + var37), var10, var14);
				emit((double)(var2 + 1), (double)((float)var3 + var18 + var20), (double)((float)var4 + var37), var10, var14);
				emit((double)(var2 + 1), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var10, var16);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 0), var12, var16);
				emit((double)(var2 + 0), (double)((float)var3 + var18 + var20), (double)((float)var4 + var37), var12, var14);
			}

			if(canCatch(var2, var3, var4 + 1)) {
				emit((double)(var2 + 1), (double)((float)var3 + var18 + var20), (double)((float)(var4 + 1) - var37), var10, var14);
				emit((double)(var2 + 1), (double)((float)(var3 + 0) + var20), (double)(var4 + 1 - 0), var10, var16);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1 - 0), var12, var16);
				emit((double)(var2 + 0), (double)((float)var3 + var18 + var20), (double)((float)(var4 + 1) - var37), var12, var14);
				emit((double)(var2 + 0), (double)((float)var3 + var18 + var20), (double)((float)(var4 + 1) - var37), var12, var14);
				emit((double)(var2 + 0), (double)((float)(var3 + 0) + var20), (double)(var4 + 1 - 0), var12, var16);
				emit((double)(var2 + 1), (double)((float)(var3 + 0) + var20), (double)(var4 + 1 - 0), var10, var16);
				emit((double)(var2 + 1), (double)((float)var3 + var18 + var20), (double)((float)(var4 + 1) - var37), var10, var14);
			}

			if(canCatch(var2, var3 + 1, var4)) {
				var21 = (double)var2 + 0.5 + 0.5;
				var23 = (double)var2 + 0.5 - 0.5;
				var25 = (double)var4 + 0.5 + 0.5;
				var27 = (double)var4 + 0.5 - 0.5;
				var29 = (double)var2 + 0.5 - 0.5;
				var31 = (double)var2 + 0.5 + 0.5;
				var33 = (double)var4 + 0.5 - 0.5;
				double var35 = (double)var4 + 0.5 + 0.5;
				var10 = (double)((float)var8 / 256.0F);
				var12 = (double)(((float)var8 + 15.99F) / 256.0F);
				var14 = (double)((float)var9 / 256.0F);
				var16 = (double)(((float)var9 + 15.99F) / 256.0F);
				++var3;
				var18 = -0.2F;
				if(((var2 + var3 + var4) & 1) == 0) {
					emit(var29, (double)((float)var3 + var18), (double)(var4 + 0), var12, var14);
					emit(var21, (double)(var3 + 0), (double)(var4 + 0), var12, var16);
					emit(var21, (double)(var3 + 0), (double)(var4 + 1), var10, var16);
					emit(var29, (double)((float)var3 + var18), (double)(var4 + 1), var10, var14);
					var10 = (double)((float)var8 / 256.0F);
					var12 = (double)(((float)var8 + 15.99F) / 256.0F);
					var14 = (double)((float)(var9 + 16) / 256.0F);
					var16 = (double)(((float)var9 + 15.99F + 16.0F) / 256.0F);
					emit(var31, (double)((float)var3 + var18), (double)(var4 + 1), var12, var14);
					emit(var23, (double)(var3 + 0), (double)(var4 + 1), var12, var16);
					emit(var23, (double)(var3 + 0), (double)(var4 + 0), var10, var16);
					emit(var31, (double)((float)var3 + var18), (double)(var4 + 0), var10, var14);
				} else {
					emit((double)(var2 + 0), (double)((float)var3 + var18), var35, var12, var14);
					emit((double)(var2 + 0), (double)(var3 + 0), var27, var12, var16);
					emit((double)(var2 + 1), (double)(var3 + 0), var27, var10, var16);
					emit((double)(var2 + 1), (double)((float)var3 + var18), var35, var10, var14);
					var10 = (double)((float)var8 / 256.0F);
					var12 = (double)(((float)var8 + 15.99F) / 256.0F);
					var14 = (double)((float)(var9 + 16) / 256.0F);
					var16 = (double)(((float)var9 + 15.99F + 16.0F) / 256.0F);
					emit((double)(var2 + 1), (double)((float)var3 + var18), var33, var12, var14);
					emit((double)(var2 + 1), (double)(var3 + 0), var25, var12, var16);
					emit((double)(var2 + 0), (double)(var3 + 0), var25, var10, var16);
					emit((double)(var2 + 0), (double)((float)var3 + var18), var33, var10, var14);
				}
			}
		} else {
			double var19 = (double)var2 + 0.5 + 0.2;
			var21 = (double)var2 + 0.5 - 0.2;
			var23 = (double)var4 + 0.5 + 0.2;
			var25 = (double)var4 + 0.5 - 0.2;
			var27 = (double)var2 + 0.5 - 0.3;
			var29 = (double)var2 + 0.5 + 0.3;
			var31 = (double)var4 + 0.5 - 0.3;
			var33 = (double)var4 + 0.5 + 0.3;
			emit(var27, (double)((float)var3 + var18), (double)(var4 + 1), var12, var14);
			emit(var19, (double)(var3 + 0), (double)(var4 + 1), var12, var16);
			emit(var19, (double)(var3 + 0), (double)(var4 + 0), var10, var16);
			emit(var27, (double)((float)var3 + var18), (double)(var4 + 0), var10, var14);
			emit(var29, (double)((float)var3 + var18), (double)(var4 + 0), var12, var14);
			emit(var21, (double)(var3 + 0), (double)(var4 + 0), var12, var16);
			emit(var21, (double)(var3 + 0), (double)(var4 + 1), var10, var16);
			emit(var29, (double)((float)var3 + var18), (double)(var4 + 1), var10, var14);
			var10 = (double)((float)var8 / 256.0F);
			var12 = (double)(((float)var8 + 15.99F) / 256.0F);
			var14 = (double)((float)(var9 + 16) / 256.0F);
			var16 = (double)(((float)var9 + 15.99F + 16.0F) / 256.0F);
			emit((double)(var2 + 1), (double)((float)var3 + var18), var33, var12, var14);
			emit((double)(var2 + 1), (double)(var3 + 0), var25, var12, var16);
			emit((double)(var2 + 0), (double)(var3 + 0), var25, var10, var16);
			emit((double)(var2 + 0), (double)((float)var3 + var18), var33, var10, var14);
			emit((double)(var2 + 0), (double)((float)var3 + var18), var31, var12, var14);
			emit((double)(var2 + 0), (double)(var3 + 0), var23, var12, var16);
			emit((double)(var2 + 1), (double)(var3 + 0), var23, var10, var16);
			emit((double)(var2 + 1), (double)((float)var3 + var18), var31, var10, var14);
			var19 = (double)var2 + 0.5 - 0.5;
			var21 = (double)var2 + 0.5 + 0.5;
			var23 = (double)var4 + 0.5 - 0.5;
			var25 = (double)var4 + 0.5 + 0.5;
			var27 = (double)var2 + 0.5 - 0.4;
			var29 = (double)var2 + 0.5 + 0.4;
			var31 = (double)var4 + 0.5 - 0.4;
			var33 = (double)var4 + 0.5 + 0.4;
			emit(var27, (double)((float)var3 + var18), (double)(var4 + 0), var10, var14);
			emit(var19, (double)(var3 + 0), (double)(var4 + 0), var10, var16);
			emit(var19, (double)(var3 + 0), (double)(var4 + 1), var12, var16);
			emit(var27, (double)((float)var3 + var18), (double)(var4 + 1), var12, var14);
			emit(var29, (double)((float)var3 + var18), (double)(var4 + 1), var10, var14);
			emit(var21, (double)(var3 + 0), (double)(var4 + 1), var10, var16);
			emit(var21, (double)(var3 + 0), (double)(var4 + 0), var12, var16);
			emit(var29, (double)((float)var3 + var18), (double)(var4 + 0), var12, var14);
			var10 = (double)((float)var8 / 256.0F);
			var12 = (double)(((float)var8 + 15.99F) / 256.0F);
			var14 = (double)((float)var9 / 256.0F);
			var16 = (double)(((float)var9 + 15.99F) / 256.0F);
			emit((double)(var2 + 0), (double)((float)var3 + var18), var33, var10, var14);
			emit((double)(var2 + 0), (double)(var3 + 0), var25, var10, var16);
			emit((double)(var2 + 1), (double)(var3 + 0), var25, var12, var16);
			emit((double)(var2 + 1), (double)((float)var3 + var18), var33, var12, var14);
			emit((double)(var2 + 1), (double)((float)var3 + var18), var31, var10, var14);
			emit((double)(var2 + 1), (double)(var3 + 0), var23, var10, var16);
			emit((double)(var2 + 0), (double)(var3 + 0), var23, var12, var16);
			emit((double)(var2 + 0), (double)((float)var3 + var18), var31, var12, var14);
		}

		return;
}

#endif

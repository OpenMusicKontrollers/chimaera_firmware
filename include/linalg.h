/*
 * Copyright (c) 2013 Hanspeter Portner (dev@open-music-kontrollers.ch)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#ifndef _LINALG_H_
#define _LINALG_H_

void linalg_solve_quadratic (float y1, float B1, float *c0, float *c1);
void linalg_solve_cubic (float y1, float B1, float y2, float B2, float *c0, float *c1, float *c2);

void linalg_least_squares_quadratic (double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1);
void linalg_least_squares_cubic (double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1, double *C2);

#endif

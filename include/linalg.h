/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _LINALG_H_
#define _LINALG_H_

void linalg_solve_quadratic(float y1, float B1, float *c0, float *c1);
void linalg_solve_cubic(float y1, float B1, float y2, float B2, float *c0, float *c1, float *c2);

void linalg_least_squares_quadratic(double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1);
void linalg_least_squares_cubic(double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1, double *C2);

#endif // _LINALG_H_

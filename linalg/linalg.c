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

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <stdio.h>

#include <linalg.h>

/*
 * algebraic solution for 3-point quadratic equation system
 *
 * y0 = C0*sqrtf(B0) + C1*B0
 * y1 = C0*sqrtf(B1) + C1*B1
 * y2 = C0*sqrtf(B2) + C1*B2
 *
 * boundary conditions:
 * y0 = B0 = 0
 * y2 = B2 = 1
 *
 * returns C0, C1
 */
void
linalg_solve_quadratic (float y1, float B1, float *c0, float *c1)
{
	float C0, C1;

	C0 = (y1 - B1) / (sqrtf(B1) - B1);
	C1 = 1.0 - C0;

	*c0 = C0;
	*c1 = C1;
}

/*
 * algebraic solution for 4-point cubic equation system
 *
 * y0 = C0*cbrtf(B0) + C1*sqrtf(B0) + C2*B0
 * y1 = C0*cbrtf(B1) + C1*sqrtf(B1) + C2*B1
 * y2 = C0*cbrtf(B2) + C1*sqrtf(B2) + C2*B2
 * y3 = C0*cbrtf(B3) + C1*sqrtf(B3) + C2*B3
 *
 * boundary conditions:
 * y0 = B0 = 0
 * y3 = B3 = 1
 *
 * returns C0, C1, C2
 */
void
linalg_solve_cubic (float y1, float B1, float y2, float B2, float *c0, float *c1, float *c2)
{
	float C0, C1, C2;
	float B1_2, B1_3, B2_2, B2_3, B1_2d, B1_3d, B2_2d, B2_3d;

	B1_2 = sqrtf(B1);
	B1_3 = cbrtf(B1);
	B2_2 = sqrtf(B2);
	B2_3 = cbrtf(B2);

	B1_2d = B1_2 - B1;
	B1_3d = B1_3 - B1;
	B2_2d = B2_2 - B2;
	B2_3d = B2_3 - B2;

	C1 = (y2 - B2 - (y1-B1)*B2_3d/B1_3d)/(B2_2d - B1_2d*B2_3d/B1_3d);
	C0 = (y1 - B1 - C1*B1_2d) / B1_3d;
	C2 = 1.0 - C0 - C1;

	*c0 = C0;
	*c1 = C1;
	*c2 = C2;
}

/*
 * algebraic least-squares fit for 5-point quadratic equation system
 *
 * y0 = C0*sqrtf(x0) + C2*x0
 * y1 = C0*sqrtf(x1) + C2*x1
 * y2 = C0*sqrtf(x2) + C2*x2
 * y3 = C0*sqrtf(x3) + C2*x3
 * y4 = C0*sqrtf(x4) + C2*x4
 *
 * boundary conditions:
 * y0 = x0 = 0
 * y4 = x4 = 1
 *
 * Mathematica is your friend:
 * returns C0, C1
 *
 * FullSimplify[
 *   Refine[
 *     LeastSquares[{{0, 0}, {a, a^2}, {b, b^2}, {c, c^2}, {1,1}}, {0, y1, y2, y3, 1}],
 *     {a, b, c} \[Element] Reals
 *   ]
 * ]
 */
void
linalg_least_squares_quadratic (double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1)
{
	double a = sqrt(x1);
	double b = sqrt(x2);
	double c = sqrt(x3);

	double a4 = x1*x1;	// = a*a*a*a;
	double a3 = x1*a;		// = a*a*a;
	double a2 = x1;			// = a*a;

	double b4 = x2*x2;	// = b*b*b*b;
	double b3 = x2*b;		// = b*b*b;
	double b2 = x2;			// = b*b;

	double c4 = x3*x3;	// = c*c*c*c;
	double c3 = x3*c;		// = c*c*c;
	double c2 = x3;			// = c*c;

	double divisor = a4*(b2+c2+1) - 2*a3*(b3+c3+1) + a2*(b4+c4+1) + b4*(c2+1) - 2*b3*(c3+1) + b2*(c4+1)+ (c-1)*(c-1)*c2;

	*C0 = (a4*(b*y2+c*y3+1) - a3*(b2*y2+c2*y3+1) - a2*y1*(b3+c3+1) + a*y1*(b4+c4+1) + (b-1)*b3 + c*y3*(b4-(b3+1)*c+1) + b*y2*(-(b*(c3+1))+c4+1) + (c-1)*c3) / divisor;
	*C1 = -(a3*(b*y2+c*y3+1) - a2*(y1*(b2+c2+1)+b2*y2+c2*y3+1) + a*y1*(b3+c3+1) + b3*(c*y3+1) - b2*(c2*y2+c2*y3+y2+1) + b*(c3+1)*y2 + (c-1)*c*(c-y3)) / divisor;
}

/*
 * algebraic least-squares fit for 5-point cubic equation system
 *
 * y0 = C0*cbrtf(x0) + C1*sqrtf(x0) + C2*x0
 * y1 = C0*cbrtf(x1) + C1*sqrtf(x1) + C2*x1
 * y2 = C0*cbrtf(x2) + C1*sqrtf(x2) + C2*x2
 * y3 = C0*cbrtf(x3) + C1*sqrtf(x3) + C2*x3
 * y4 = C0*cbrtf(x4) + C1*sqrtf(x4) + C2*x4
 *
 * boundary conditions:
 * y0 = x0 = 0
 * y4 = x4 = 1
 *
 * returns C0, C1
 *
 * Mathematica is your friend:
 *  FullSimplify[
 *    Refine[
 *      LeastSquares[{{0, 0, 0}, {a^2, a^3, a^6}, {b^2, b^3, b^6}, {c^2, c^3, c^6}, {1, 1, 1}}, {0, y1, y2, y3, 1}],
 *      {a, b, c} \[Element] Reals
 *    ]
 *  ]
 */
void
linalg_least_squares_cubic (double x1, double y1, double x2, double y2, double x3, double y3, double *C0, double *C1, double *C2)
{
	double a12= pow(x1, 12.0/6.0);
	double a9 = pow(x1, 9.0/6.0);
	double a8 = pow(x1, 8.0/6.0);
	double a7 = pow(x1, 7.0/6.0);
	double a6 = pow(x1, 6.0/6.0);
	double a5 = pow(x1, 5.0/6.0);
	double a4 = pow(x1, 4.0/6.0);
	double a3 = pow(x1, 3.0/6.0);
	double a2 = pow(x1, 2.0/6.0);
	double a  = pow(x1, 1.0/6.0);

	double b12= pow(x2, 12.0/6.0);
	double b9 = pow(x2, 9.0/6.0);
	double b8 = pow(x2, 8.0/6.0);
	double b7 = pow(x2, 7.0/6.0);
	double b6 = pow(x2, 6.0/6.0);
	double b5 = pow(x2, 5.0/6.0);
	double b4 = pow(x2, 4.0/6.0);
	double b3 = pow(x2, 3.0/6.0);
	double b2 = pow(x2, 2.0/6.0);
	double b  = pow(x2, 1.0/6.0);

	double c12= pow(x3, 12.0/6.0);
	double c9 = pow(x3, 9.0/6.0);
	double c8 = pow(x3, 8.0/6.0);
	double c7 = pow(x3, 7.0/6.0);
	double c6 = pow(x3, 6.0/6.0);
	double c5 = pow(x3, 5.0/6.0);
	double c4 = pow(x3, 4.0/6.0);
	double c3 = pow(x3, 3.0/6.0);
	double c2 = pow(x3, 2.0/6.0);
	double c  = pow(x3, 1.0/6.0);

	double divisor = b4*c4*pow(b+b4*(-1+c)-c+c4-b*c4,2.0)+a12*(pow(-1+c,2.0)*c4+b6*(1+c4)-2*b5*(1+c5)+b4*(1+c6))-2*a9*(pow(-1+c,2)*c4*(1+c)*(1+c2)+b9*(1+c4)-b8*(1+c5)-b5*(1+c8)+b4*(1+c9))+2*a8*(pow(-1+c,2.0)*c5*(1+c+c2)+b9*(1+c5)-b8*(1+c6)-b6*(1+c8)+b5*(1+c9))+a6*(c4*pow(-1+c4,2.0)+b12*(1+c4)-2*b8*(1+c8)+b4*(1+c12))-2*a5*(c5+c8*(-1-c+c4)+b12*(1+c5)-b9*(1+c8)-b8*(1+c9)+b5*(1+c12))+a4*(c6*pow(-1+c3,2.0)+b12*(1+c6)-2*b9*(1+c9)+b6*(1+c12));

	*C0 = (a3*((-b12)*(1+c5)+c5*(-1+c3+c4-c7)+b9*(1+c8)+b8*(1+c9)-b5*(1+c12))*y1+a2*(c6*pow(-1+c3,2.0)+b12*(1+c6)-2*b9*(1+c9)+b6*(1+c12))*y1+a12*((-1+b)*b5+(-1+c)*c5+b2*(1+c6-b*(1+c5))*y2+c2*(1+b6-(1+b5)*c)*y3)+a5*(b9-b12+c9-c12+b6*(1+c9)*y2-b3*(1+c12)*y2+c3*(-1-b12+(1+b9)*c3)*y3)+a9*(c5+c8-2*c9+b2*(b3+b6-2*b7+b4*(1+c5)*y2+b*(1+c8)*y2-2*(1+c9)*y2)+c2*(-2-2*b9+c+b8*c+(1+b5)*c4)*y3)+(-1+b)*b2*(-1+c)*c2*(b+b2+b3-c*(1+c+c2))*(c3*(-1+c3)*y2+b6*(c3-y3)+b3*(-c6+y3))+a6*(c12+c5*y1+c9*y1+b9*(1+c5)*y1+b5*(1+c9)*y1-c8*(1+y1)+b2*(1+c12)*y2-b6*(1+c8)*(y1+y2)+c2*y3-c6*(y1+y3)+b12*(1+c2*y3)-b8*(1+y1+c6*y1+c6*y3))+a8*(b3*(1+c9)*y2+c3*(-1+c3)*(c3-y3)+b9*(1+c3*y3)-b6*(1+y2+c6*y2+c6*y3))) / divisor;

	*C1 = (a6*((-b9)*(1+c4)+c4*(-1+c+c4-c5)+b8*(1+c5)+b5*(1+c8)-b4*(1+c9))*y1+a3*(c4*pow(-1+c4,2.0)+b12*(1+c4)-2*b8*(1+c8)+b4*(1+c12))*y1+a2*((-b12)*(1+c5)+c5*(-1+c3+c4-c7)+b9*(1+c8)+b8*(1+c9)-b5*(1+c12))*y1+a4*(c9*(-1+c3)+b3*(-b6+b9+y2+c12*y2-b3*(1+c9)*y2)+c3*(1+b12-(1+b9)*c3)*y3)+a9*(-c4+c8+b2*(-b2+b6+y2+c8*y2-b4*(1+c4)*y2)+c2*(1+b8-(1+b4)*c4)*y3)+a8*(b5-2*b8+b9+c5-2*c8+c9+b2*(1+c9+b4*(1+c5)-2*b*(1+c8))*y2+c2*(1+b9-2*(1+b8)*c+(1+b5)*c4)*y3)+a5*(b8-b12+c8-c12+b6*(1+c8)*y2-b2*(1+c12)*y2+c2*(-1-b12+(1+b8)*c4)*y3)+a12*((-(-1+b))*b4-(-1+c)*c4+b2*(-1+b+b*c4-c5)*y2+c2*(-1+c+b4*(-b+c))*y3)-(-1+b)*b2*(-1+c)*c2*(b+b2+b3-c*(1+c+c2))*(c2*(-1+c4)*y2+b6*(c2-y3)+b2*(-c6+y3))) / divisor;

	*C2 = (a3*((-b9)*(1+c4)+c4*(-1+c+c4-c5)+b8*(1+c5)+b5*(1+c8)-b4*(1+c9))*y1+a2*(pow(-1+c,2.0)*c5*(1+c+c2)+b9*(1+c5)-b8*(1+c6)-b6*(1+c8)+b5*(1+c9))*y1+(-1+b)*b2*(-1+c)*c2*(b+b2+b3-c*(1+c+c2))*(c2*(b3-b2*c+(-1+c)*y2)-(-1+b)*b2*y3)+a9*((-1+b)*b4+(-1+c)*c4+b2*(1+c5-b*(1+c4))*y2+c2*(1+b5-(1+b4)*c)*y3)+a5*(-2*c5+c8+c9+b2*(-2*b3+b6+b7+y2+c9*y2-2*b4*(1+c5)*y2+b*(1+c8)*y2)+c2*(1+b9+c+b8*c-2*(1+b5)*c4)*y3)+a8*((-(-1+b))*b5-(-1+c)*c5+b2*(-1+b+b*c5-c6)*y2+c2*(-1+c+b5*(-b+c))*y3)+a6*(-2*b5*(1+c5)*y1+c4*(1-c4+pow(-1+c,2.0)*y1)-b2*(1+c8)*y2+b6*(1+c4)*(y1+y2)+c2*(-1+c4)*y3-b8*(1+c2*y3)+b4*(1+y1+c6*y1+c6*y3))+a4*((-b3)*(1+c9)*y2-c3*(-1+c3)*(c3-y3)-b9*(1+c3*y3)+b6*(1+y2+c6*y2+c6*y3))) / divisor;
}

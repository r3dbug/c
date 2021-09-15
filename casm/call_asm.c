
/*
 * Simple demonstration about how to call assembly
 * functions from C using SAS/C and VASM 
 *
 * call_asm.c:      main program
 * asm_functions:   assembly functions
 * smakefile:       makefile (for smake which is part of SAS/C)
 *
 * compile with:
 * smake all
 */
 
#include <stdio.h>

/*
 * Calculate fibonacci(n) = f(n):
 * f(0) = 0;
 * f(1) = 1;
 * f(n) = f(n-2)+f(n-1) with n>=2
 *
 * Parameter n is passed via register d0.
 * Return value f(n) in register d0
 *
 * Keywords: __asm, register, __d0 (double underscore)
 *
 * Declare function prototype with function name
 *
 */
 
unsigned long int __asm fibonacci(
     register __d0 int x
);

/*
 * Calculate average(x,y)=(x+y)/2
 * 
 * Floating point parameters are passed via registers.
 * Return value in register fp0.
 *
 */
 
double __asm average(
    register __fp0 double x,
    register __fp1 double y
);

/*
 * Calculate multiply(x,y)=x*y
 * 
 * Floating point parameters are passed via stack.
 * Return value in register fp0.
 *
 */

double multiply(
    double x,
    double y
);

int main(void) {

    unsigned long int n, result;
    double a=1.2895, b=6.829374, c;
    
    // fibonacci (integer, via register)
    for (n=0; n<45; n++) {
        result=fibonacci(n);
        printf("Fibonacci %lu: %lu\n",n,result);
    }
    
    // average (floating point, via registers)
    // with 2 doubles
    c=average(a,b);
    printf("average(%lf,%lf)=%lf\n",a,b,c);
    
    // with integer+double
    n=10;
    c=average((double)n,b);
    printf("average(%d,%lf)=%lf\n",n,b,c);
    
    //multiply
    c=multiply(a,b);
    printf("multiply(%lf,%lf)=%lf\n",a,b,c);
    
    return 0;
}








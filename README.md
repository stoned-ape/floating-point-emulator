# Software Floating Point Emulator

Use `make` to build or `make runbin` to build and run test cases.

## Features

* Addition
* Subtraction
* Multiplication
* Division
* Square Roots

## Limitations
* Only 32 bit floats are supported (no doubles, halfs, or long doubles)
* Only supports the FE_TONEAREST rounding mode
* The accuracy for multiplication and division are slighly off for some test cases 
	* multiplication result is no more than 3 minimum possible steps from away from the true value
	* division result is not more than two steps away


## Implementation

This Implementation attempts to conform to the IEEE-754 floating point standard.
The following explanation captures the spirit of the implementation
while avoiding the tedious and gory details seen in the code.

We can represent a float x, with two ints a,b in the following way.

```c
int a,b,c,d;
float x=a*2**b;
float y=c*2**d;
```
Where `2**b` means 2 raised to the power of b.
### Addition and Subtraction
```c
if(x>y){
	x+y=(a+(c>>(b-d)))*2**b;
	x-y=(a-(c>>(b-d)))*2**b;
}
```
### Multiplication
```c
x*y=(a*c)*2**(b+d);
```
### Division
```c
x/y=(a/c)*2**(b-d);
```
Where / indicates integer division
### Square Roots
```c
sqrt(x)=int_sqrt(a)*2**(b/2);
```


















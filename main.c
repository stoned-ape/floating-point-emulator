#ifdef __cplusplus
#pragma message "C++"
extern "C"{
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <fenv.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

//returns true if the distance between a and b is
//at most idx smallest possible steps
bool dist_lt(float a,float b,int idx){
	for(int i=0;i<idx;i++){
		a=nextafterf(a,b);
		if(a==b) return true;
	}
	return false;
}

//prints a floating point exception returned by fetestexcept()
void print_exception(int exc){
	switch(exc){
		case FE_INEXACT         :puts("FE_INEXACT        ");break;
		case FE_UNDERFLOW       :puts("FE_UNDERFLOW      ");break;
		case FE_OVERFLOW        :puts("FE_OVERFLOW       ");break;
		case FE_DIVBYZERO       :puts("FE_DIVBYZERO      ");break;
		case FE_INVALID         :puts("FE_INVALID        ");break;
		case FE_ALL_EXCEPT      :puts("FE_ALL_EXCEPT     ");break;
		#ifdef __APPLE__
		case FE_DENORMALOPERAND :puts("FE_DENORMALOPERAND");break;
		#endif
		case 0:  puts("no exception");break;
		default: puts("invalid exception");
	}
}

//a type cast that doesnt modify the bits
#define bit_cast(type,f) (*(type*)&(f))

//returns the n'th bit of x (zero indexed)
#define get_bit(x,n)   (bit_cast(uint32_t     ,x)<<(31-(n))>>31)
#define get_bit64(x,n) (bit_cast(uint64_t,x)<<(63-(n))>>63)


//returns a string binary representation of a 32 bit value
char *string32(void *v){
	uint32_t *xp=(uint32_t*)v;
	uint32_t x=*xp;
	char *c=(char*)malloc(33);
	c[32]=0;
	for(int i=0;i<32;i++){
		c[31-i]=x%2+'0';
		x>>=1;
	}
	return c;
}

void _bprint32(uint32_t x){
	char *c=string32(&x);
	puts(c);
	free(c);
}

#define bprint32(x) _bprint32(bit_cast(uint32_t,x))

//shifts up or down depending on the sign of "shift"
//the operator "<<" doesnt return zero if the shift amount
//is greater than the number of bits in x for some reason
//this avoids that unexpected behavior
uint32_t logical_shift(uint32_t x,int shift){
	if(abs(shift)<32) return shift<0?x>>-shift:x<<shift;
	return 0;
}
uint64_t logical_shift64(uint64_t x,int shift){
	if(abs(shift)<64) return shift<0?x>>-shift:x<<shift;
	return 0;
}

//convert an integer to an equivalent float in software
float i2f(int xi){
	uint32_t y=0;
	if(xi<0){
		y|=(1u<<31);
		xi*=-1;
	}
	uint32_t x=bit_cast(uint32_t,xi);
	int high=-127;
	for(int i=30;i>=0;i--){
		if(get_bit(x,i)){
			high=i;
			break;
		}
	}
	y|=(127+high)<<23;
	x=logical_shift(x,31-high+1);
	x>>=9;
	y|=x;
	return bit_cast(float,y);
}

bool i2f_test(int x){
	float f1=x;
	float f2=i2f(x);
	if(f1!=f2){
		char *s1=string32(&f1);
		char *s2=string32(&f2);
		printf("cast error on %d:\n%s vs \n%s\n",x,s1,s2);
		free(s1);
		free(s2);
		return true;
	}
	return false;
}


//extract the fractional an exponential components of a float
//in the float representation, the leading bit of the fractional
//part is ommitted, so here we must add it back in
uint32_t get_exp (uint32_t x){return (x<<1)>>24;}
uint32_t get_frac(uint32_t x){return ((x<<9)>>1)|(1u<<31);}

uint64_t get_exp64 (uint64_t x){return (x<<(1+32))>>(24+32);}
uint64_t get_frac64(uint64_t x){return ((x<<(9+32))>>1)|(1ull<<(63));}

//binary representations of special floating point values
uint32_t _NAN=0b01111111110000000000000000000000u;
uint32_t SNAN=0b11111111110000000000000000000000u;
uint32_t _INF=0b01111111100000000000000000000000u;
uint32_t MINF=0b11111111100000000000000000000000u;
uint32_t _ONE=0b00111111100000000000000000000000u;
uint32_t MONE=0b10111111100000000000000000000000u;
uint32_t MZRO=0b10000000000000000000000000000000u;

int f2i(float f){
	uint32_t x=bit_cast(uint32_t,f);
	uint32_t y=0;
	if(x==_NAN || x==_INF || x==MINF) return 1u<<31;
	uint32_t exp=get_exp(x),frac=get_frac(x);
	int shift=(int)exp-127-31;
	y|=logical_shift(frac,shift);
	if(get_bit(x,31)) y=~y+1;
	return bit_cast(int,y);
}

bool f2i_test(float x){
	feclearexcept(FE_ALL_EXCEPT); //discard any previous exceptions
	int i1=x;
	int exc=fetestexcept(FE_ALL_EXCEPT); //save current exception
	int i2=f2i(x);
	if(i1!=i2){
		char *s1=string32(&i1);
		char *s2=string32(&i2);
		printf("cast error on %f:\n%s =real \n%s =emu\n",x,s1,s2);
		print_exception(exc);
		free(s1);
		free(s2);
		return true;
	}
	return false;
}

//clears all bits up to but not including bit i
//rounds up
uint64_t roundup_ul(uint64_t x,uint32_t i){
	assert(i<64 && i>0);
	uint64_t round_bit=get_bit64(x,i-1);
	uint64_t res=((x>>i)<<i)+(round_bit<<i);
	return res;
}


//creates a float from a sign, exponent, and fraction
//also handles rounding
//float rounding is strange.  
//Analogy: 
//	Say we need to round 4.5 to the nearest int
//	4 and 5 equally close thus we have a tie.
//	we resolve this tie by chosing the even option
//	so 4.5 rounds to 4, but 3.5 also rounds 4.
uint32_t assemble_float(uint64_t sign,uint64_t exp,uint64_t frac){
	assert((sign>>1)==0);
	assert((exp>>8)==0);
	frac>>=8;
	uint64_t x=0;
	uint64_t rounded=0,b24=0,cleared=(frac>>32)<<32;
	rounded=roundup_ul(frac,32);
	b24=get_bit64(rounded,24+32);
	while(b24){
		rounded>>=1;
		exp+=1;
		rounded=roundup_ul(rounded,32);
		b24=get_bit64(rounded,24+32);
	}
	if(exp>255) return sign?MINF:_INF;
	if(rounded!=cleared){
		if(rounded-frac==frac-cleared){ // rounding tie
			rounded=get_bit64(rounded,32)?cleared:rounded; //choose the even option
		}
	}
	rounded>>=32;
	rounded&=~(get_bit64(rounded,23)<<(23)); //remove the bit before the point
	x|=sign<<31;
	x|=exp<<23;
	x|=rounded;
	return x;
}

float u2f(uint32_t u){return bit_cast(float,u);}

float faddsub(float fa,float fb,bool add){
	uint64_t a=bit_cast(uint32_t,fa),b=bit_cast(uint32_t,fb);
	uint64_t c=0;
	uint64_t aexp=get_exp64(a),bexp=get_exp64(b);
	uint64_t asign=get_bit64(a,31),bsign=!add^get_bit64(b,31);
	uint64_t afrac=get_frac64(a)>>1,bfrac=get_frac64(b)>>1;
	uint64_t cexp=0,cfrac=0,csign=0;
	if(a==_NAN || b==_NAN) return bit_cast(float,_NAN);
	if(a==_INF && b==MINF) return add?bit_cast(float,_NAN):bit_cast(float,_INF);
	if(a==MINF && b==_INF) return add?bit_cast(float,_NAN):bit_cast(float,MINF);
	if(a==_INF && b==_INF) return add?bit_cast(float,_INF):bit_cast(float,_NAN);
	if(a==MINF && b==MINF) return add?bit_cast(float,MINF):bit_cast(float,_NAN);
	if(!add) b=get_bit64(b,31)?b&~(1u<<31):b|(1u<<31); //negate b
	if(a==_INF || b==_INF) return bit_cast(float,_INF);
	if(a==MINF || b==MINF) return bit_cast(float,MINF);
	if(a==MZRO && b==MZRO) return bit_cast(float,MZRO);
	if     (a==0) c=b;
	else if(b==0) c=a;
	else if(asign==bsign){
		csign=asign;
		if(aexp==bexp){
			cexp=aexp+1;
			cfrac=afrac+bfrac;	
		}else if(aexp>bexp){
			cexp=aexp+1;
			cfrac=afrac+logical_shift64(bfrac,-(int)(aexp-bexp));
		}else if(aexp<bexp){
			cexp=bexp+1;
			cfrac=bfrac+logical_shift64(afrac,-(int)(bexp-aexp));
		}
		while(cfrac!=0 && !get_bit64(cfrac,63)){
			cexp--;
			cfrac<<=1;
		}
		c=assemble_float(csign,cexp,cfrac);
	}else{
		if(aexp==bexp){
			cfrac=(afrac>bfrac)?afrac-bfrac:bfrac-afrac;
			cexp=cfrac?aexp+1:0;
			csign=(uint64_t)((afrac<bfrac)!=asign);
		}else if(aexp>bexp){
			cexp=aexp+1;
			cfrac=afrac-logical_shift64(bfrac,-(int)(aexp-bexp));
			csign=asign;
		}else if(aexp<bexp){
			cexp=bexp+1;
			cfrac=bfrac-logical_shift64(afrac,-(int)(bexp-aexp));
			csign=bsign;
		}
		while(cfrac!=0 && !get_bit64(cfrac,63)){
			cexp--;
			cfrac<<=1;
		}
		c=assemble_float(csign,cexp,cfrac);
	}

	return bit_cast(float,c);
}

bool is_nan(float x){return x!=x;}


bool faddsub_test(float a,float b,bool add){
	feclearexcept(FE_ALL_EXCEPT);
	float f1=add?a+b:a-b;
	int exc=fetestexcept(FE_ALL_EXCEPT);
	float f2=faddsub(a,b,add);
	if(f1!=f2){
		if(is_nan(f1) && is_nan(f2)) return false;
		uint32_t xor=bit_cast(uint32_t,f1)^bit_cast(uint32_t,f2);
		// if(!(xor>>1)) return false;
		char *s1=string32(&f1);
		char *s2=string32(&f2);
		char *s3=string32(&xor);
		printf("addition error on %e %c %e, diff=%e\n",a,add?'+':'-',b,fabsf(f1-f2));
		if(get_bit(xor,31)) printf("\tsign error ");
		if((xor<<1)>>24)    printf("\texp error ");
		if(xor<<8)          printf("\tfrac error");
		printf("\n\t%s =real \n\t%s =emu \n\t%s =diff \n",s1,s2,s3);
		printf("\t");
		print_exception(exc);
		free(s1);
		free(s2);
		free(s3);
		return true;
	}
	return false;
}


bool feq(float fa,float fb){
	uint32_t a=bit_cast(uint32_t,fa),b=bit_cast(uint32_t,fb);
	if(a==_NAN || b==_NAN) return false;
	if(a==MZRO && b==0   ) return true;
	if(a==0    && b==MZRO) return true;
	return a==b; 
}

bool feq_test(float a,float b){
	bool b1=a==b,b2=feq(a,b);
	if(b1!=b2){
		printf("equality error on %e == %e",a,b);
		printf("\n%d =real \n%d =emu \n",b1,b2);
		return true;
	}
	return false;
}

bool flt(float fa,float fb){
	uint32_t a=bit_cast(uint32_t,fa),b=bit_cast(uint32_t,fb);
	uint32_t aexp=get_exp(a),bexp=get_exp(b);
	uint32_t asign=get_bit(a,31),bsign=get_bit(b,31);
	uint32_t afrac=get_frac(a)>>9,bfrac=get_frac(b)>>9;
	if(a==_NAN || b==_NAN) return false;
	if(a==MZRO && b==0   ) return false;
	if(asign!=bsign) return asign && !bsign;
	if(aexp!=bexp) return asign?aexp>bexp:aexp<bexp;
	return asign?afrac>bfrac:afrac<bfrac;
}

bool flt_test(float a,float b){
	bool b1=a<b,b2=flt(a,b);
	if(b1!=b2){
		printf("less than error on %e < %e",a,b);
		printf("\n%d =real \n%d =emu \n",b1,b2);
		return true;
	}
	return false;
}




//mutliply the fractions and add the exponents 
float fmul(float fa,float fb){
	uint64_t a=bit_cast(uint32_t,fa),b=bit_cast(uint32_t,fb);
	uint32_t c=0;
	uint64_t aexp=get_exp64(a),bexp=get_exp64(b);
	uint64_t asign=get_bit64(a,31),bsign=get_bit64(b,31);
	uint64_t afrac=get_frac(a)>>9,bfrac=get_frac(b)>>9;
	uint64_t cfrac=afrac*bfrac;
	assert(cfrac/afrac==bfrac);
	assert(cfrac/bfrac==afrac);
	uint64_t cexp=(aexp+bexp)+256-127+19,csign=asign^bsign;
	if(a==_NAN || b==_NAN) return bit_cast(float,_NAN);
	if(a==_INF && b==MINF) return bit_cast(float,MINF);
	if(a==MINF && b==_INF) return bit_cast(float,MINF);
	if(a==_INF && b==_INF) return bit_cast(float,_INF);
	if(a==MINF && b==MINF) return bit_cast(float,_INF);
	if(a==_INF && b==0   ) return bit_cast(float,_NAN);
	if(a==0    && b==_INF) return bit_cast(float,_NAN);
	if(a==MINF && b==0   ) return bit_cast(float,_NAN);
	if(a==0    && b==MINF) return bit_cast(float,_NAN);
	if(a==_INF && b==MZRO) return bit_cast(float,_NAN);
	if(a==MZRO && b==_INF) return bit_cast(float,_NAN);
	if(a==MINF && b==MZRO) return bit_cast(float,_NAN);
	if(a==MZRO && b==MINF) return bit_cast(float,_NAN);
	if(a==_INF || b==_INF) return csign?bit_cast(float,MINF):bit_cast(float,_INF);
	if(a==MINF || b==MINF) return csign?bit_cast(float,MINF):bit_cast(float,_INF);
	if(a==MZRO || b==MZRO) return 0;
	if(a==0    || b==0   ) return 0;
	assert(a!=MZRO);
	assert(b!=MZRO);
	while(cfrac!=0 && !get_bit64(cfrac,63)){
		cfrac<<=1;
		cexp--;
		assert(cexp!=0);
	}
	assert(cexp>=256);
	cexp-=256;
	c=assemble_float(csign,cexp,cfrac);
	return bit_cast(float,c);
}

bool fmul_test(float a,float b){
	feclearexcept(FE_ALL_EXCEPT);
	float f1=a*b;
	int exc=fetestexcept(FE_ALL_EXCEPT);
	float f2=fmul(a,b);
	if(f1!=f2){
		if(is_nan(f1) && is_nan(f2)) return false;
		uint32_t xor=bit_cast(uint32_t,f1)^bit_cast(uint32_t,f2);
		if(dist_lt(f1,f2,3)) return false;
		char *s1=string32(&f1);
		char *s2=string32(&f2);
		char *s3=string32(&xor);
		char *as=string32(&a);
		char *bs=string32(&b);
		printf("multiplication error on %e * %e, diff=%e\n",a,b,fabsf(f1-f2));
		printf("\t%s =a\n",as);
		printf("\t%s =b\n",bs);
		if(get_bit(xor,31)) printf("\tsign error ");
		if((xor<<1)>>24) printf("\texp error ");
		if(xor<<8) printf("\tfrac error");
		puts("");
		printf("\t%s =real \n\t%s =emu \n\t%s =diff \n",s1,s2,s3);
		printf("\t");
		print_exception(exc);
		free(as);
		free(bs);
		free(s1);
		free(s2);
		free(s3);
		return true;
	}
	return false;
}



//divide the factions and subtract the exponents
float fdiv(float fa,float fb){
	uint64_t a=bit_cast(uint32_t,fa),b=bit_cast(uint32_t,fb);
	uint32_t c=0;
	int64_t aexp=get_exp64(a),bexp=get_exp64(b);
	uint64_t asign=get_bit64(a,31),bsign=get_bit64(b,31);
	uint64_t afrac=get_frac64(a),bfrac=get_frac(b)>>9;
	// aexp-=63;
	bexp-=31-9;
	uint64_t csign=asign^bsign;
	if(a==_NAN || b==_NAN) return bit_cast(float,_NAN);
	if(a==_INF && b==MINF) return bit_cast(float,_NAN);
	if(a==MINF && b==_INF) return bit_cast(float,_NAN);
	if(a==_INF && b==_INF) return bit_cast(float,_NAN);
	if(a==MINF && b==MINF) return bit_cast(float,_NAN);
	if(a==_INF && b==0   ) return bit_cast(float,_INF);
	if(a==0    && b==_INF) return u2f(0);
	if(a==0    && b==MINF) return u2f(1u<<31);
	if(a==0    && b==0   ) return bit_cast(float,SNAN);
	if(a==_INF && b==MZRO) return bit_cast(float,MINF);
	if(a==MZRO && b==_INF) return u2f(0);
	if(a==MZRO && b==MINF) return u2f(1u<<31);
	if(a==MZRO && b==MZRO) return bit_cast(float,SNAN);
	if(a==MZRO && b==0   ) return bit_cast(float,SNAN);
	if(a==0    && b==MZRO) return bit_cast(float,SNAN);
	if(b==_INF || b==MINF) return csign?u2f(1u<<31):u2f(0);
	if(a==_INF || a==MINF) return csign?bit_cast(float,MINF):bit_cast(float,_INF);
	if(b==0    || b==MZRO) return csign?bit_cast(float,MINF):bit_cast(float,_INF);
	if(a==0    || a==MZRO) return csign?u2f(1u<<31):u2f(0);
	while(bfrac!=0 && !get_bit64(bfrac,0)){
		bfrac>>=1;
		bexp++;
	}
	uint64_t cfrac=afrac/bfrac;
	int64_t cexp=((aexp-127)-(bexp-127))+127;

	while(cfrac!=0 && !get_bit64(cfrac,63)){
		cfrac<<=1;
		cexp--;
		assert(cexp!=0);
	}
	cexp=cexp>0?cexp:0;
	c=assemble_float(csign,(uint64_t)cexp,cfrac);
	return bit_cast(float,c);
}




bool fdiv_test(float a,float b){
	feclearexcept(FE_ALL_EXCEPT);
	float f1=a/b;
	int exc=fetestexcept(FE_ALL_EXCEPT);
	float f2=fdiv(a,b);
	if(f1!=f2){
		if(is_nan(f1) && is_nan(f2)) return false;
		uint32_t xor=bit_cast(uint32_t,f1)^bit_cast(uint32_t,f2);
		// if(!(xor>>1)) return false;
		if(dist_lt(f1,f2,2)) return false;
		char *s1=string32(&f1);
		char *s2=string32(&f2);
		char *s3=string32(&xor);
		char *as=string32(&a);
		char *bs=string32(&b);
		printf("division error on %e / %e, diff=%e\n",a,b,fabsf(f1-f2));
		printf("\t%s =a\n",as);
		printf("\t%s =b\n",bs);
		if(get_bit(xor,31)) printf("\tsign error ");
		if((xor<<1)>>24) printf("\texp error ");
		if(xor<<8) printf("\tfrac error");
		puts("");
		printf("\t%s =real \n\t%s =emu \n\t%s =diff \n",s1,s2,s3);
		printf("\t");
		print_exception(exc);
		free(as);
		free(bs);
		free(s1);
		free(s2);
		free(s3);
		return true;
	}
	return false;
}

//O(sqrt(x)) can we do better?
//only slighly better than a linear search (no multiplies)
int64_t lin_sqrt(int64_t x){
	int64_t i=0;
	while(x>0){
		x-=(i<<2)+1;
		i++;
	}
	return i;
}

//O(log2(x)), much better
uint64_t bin_sqrt(uint64_t x){
	uint64_t lo=0,hi=x+1,mid=0;
	uint64_t p=4;
	while(x>p && p<ULONG_MAX>>2){
		hi>>=1;
		p<<=2;
	}
	while(lo<hi-1){
		mid=lo+(hi-lo)/2;
		// printf("%ld \t %ld \t %ld\n",lo,mid,hi);
		uint64_t sq=mid*mid;
		if(sq==x) return mid;
		if(sq<x) lo=mid;
		else     hi=mid;
	}
	return mid;
}

//divide the exponent by two and compute the integer square
//root of the fractional part
float fsqrt(float fx){
	uint64_t x=bit_cast(uint32_t,fx);
	int64_t xexp=get_exp64(x);
	uint64_t xsign=get_bit64(x,31);
	int shift=1;
	uint64_t xfrac=get_frac64(x)>>shift;
	xexp-=(63-shift);
	uint32_t y=0;
	int64_t yexp=0;
	uint64_t yfrac=0;
	if(x==_NAN) return bit_cast(float,_NAN);
	if(x==0   ) return u2f(0);
	if(x==MZRO) return bit_cast(float,MZRO);
	if(xsign  ) return bit_cast(float,_NAN);
	if(x==_INF) return bit_cast(float,_INF);
	if(x==_ONE) return bit_cast(float,_ONE);
	bool is_odd=!(xexp&1);
	xexp+=is_odd;
	xfrac>>=is_odd;
	yexp=((xexp-127)/2)+127+63;
	yfrac=bin_sqrt(xfrac);
	assert(yexp>0);
	while(yfrac!=0 && !get_bit64(yfrac,63)){
		yfrac<<=1;
		yexp-=1;
		assert(yexp!=0);
	}
	y=assemble_float(0,(uint64_t)yexp,yfrac);
	return bit_cast(float,y);
}

bool fsqrt_test(float a){
	feclearexcept(FE_ALL_EXCEPT);
	float f1=sqrtf(a);
	int exc=fetestexcept(FE_ALL_EXCEPT);
	float f2=fsqrt(a);
	if(f1!=f2){
		if(is_nan(f1) && is_nan(f2)) return false;
		uint32_t rf1=bit_cast(uint32_t,f1),rf2=bit_cast(uint32_t,f2);
		uint32_t xor=rf1^rf2;
		// if(!(xor<<9)) return false;
		// if(dist_lt(f1,f2,2)) return false;
		char *s1=string32(&f1);
		char *s2=string32(&f2);
		char *s3=string32(&xor);
		char *as=string32(&a);
		printf("sqrt error on sqrt( %e ) diff=%e\n",a,fabsf(f1-f2));
		printf("\t%s =a\n",as);
		if(get_bit(xor,31)) printf("\tsign error ");
		if((xor<<1)>>24) printf("\texp error ");
		if(xor<<8) printf("\tfrac error");
		puts("");
		printf("\t%s =real \n\t%s =emu \n\t%s =diff \n",s1,s2,s3);
		printf("\t");
		print_exception(exc);
		printf("\texp diff: %d \n",(int)(rf2>>23)-(int)(rf1>>23));
		free(as);
		free(s1);
		free(s2);
		free(s3);
		return true;
	}
	return false;
}


//prints the cpu's current rounding mode
//this emulator only supports FE_TONEAREST
void print_rounding_mode(){
	switch(fegetround()){
		case FE_TONEAREST  :puts("FE_TONEAREST ");break;
		case FE_UPWARD     :puts("FE_UPWARD    ");break;
		case FE_DOWNWARD   :puts("FE_DOWNWARD  ");break;
		case FE_TOWARDZERO :puts("FE_TOWARDZERO");break;
	}
}



int main(){
	for(int64_t i=0;i<10000;i++){
		int64_t rt=bin_sqrt(i*i);
		// printf("sqrt(%ld) = %ld\n",i*i,rt);
		assert(rt==i);
	}
	print_rounding_mode();
	float tests[]={
		+0,+1,+2,+3,+4,+5,+6,+7,+8,+9,
		-0,-1,-2,-3,-4,-5,-6,-7,-8,-9,
		+1./0,+1./2,+1./3,+1./4,+1./5,+1./6,+1./7,+1./8,+1./9,
		-1./0,-1./2,-1./3,-1./4,-1./5,-1./6,-1./7,-1./8,-1./9,
		+1e+2,+1e+3,+1e+4,+1e+5,+1e+6,+1e+7,+1e+8,+1e+9,
		+1e-2,+1e-3,+1e-4,+1e-5,+1e-6,+1e-7,+1e-8,+1e-9,
		-1e+2,-1e+3,-1e+4,-1e+5,-1e+6,-1e+7,-1e+8,-1e+9,
		-1e-2,-1e-3,-1e-4,-1e-5,-1e-6,-1e-7,-1e-8,-1e-9,		
		+0./0,1./0,-1.0/0,0.,1,-0.,//1e38,1e39,-1/7*1e38,1e40,//1e-40
	};
	int n=sizeof(tests)/sizeof(float);
	int fails=0,ntests=0;
	puts("testing i2f");
	for(int i=-1000;i<1000;i++){
		fails+=i2f_test(i);
		ntests++;
	}
	puts("testing f2i");
	for(int i=0;i<n;i++){
		if(tests[i]==1./0 || tests[i]==-1./0 || tests[i]!=tests[i]) 
			continue; //undefined behavior
		fails+=f2i_test(tests[i]);
		ntests++;
	}
	puts("testing feq");
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			fails+=feq_test(tests[i],tests[j]);
			ntests++;
		}
	}
	puts("testing flt");
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			fails+=flt_test(tests[i],tests[j]);
			ntests++;
		}
	}
	puts("testing faddsub");
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			for(int b=0;b<=1;b++){
				fails+=faddsub_test(tests[i],tests[j],(bool)b);
				ntests++;
			}
		}
	}
	assert(fails==0);
	puts("testing fmul");
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			fails+=fmul_test(tests[i],tests[j]);
			ntests++;
		}
	}
	puts("testing fdiv");
	for(int i=0;i<n;i++){
		for(int j=0;j<n;j++){
			fails+=fdiv_test(tests[i],tests[j]);
			ntests++;
		}
	}
	assert(fails==0);
	// fails=0;
	// ntests=0;
	puts("testing fsqrt");
	for(int i=0;i<n;i++){
		fails+=fsqrt_test(tests[i]);
		ntests++;
	}
	printf("%d tests failed out of %d\n",fails,ntests);
	return fails;
}










#ifdef __cplusplus
}
#endif



//

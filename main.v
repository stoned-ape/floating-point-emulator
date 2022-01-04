
`define get_bit(x,idx) (((x)<<(31-(idx)))>>31)

`define _NAN 32'b01111111110000000000000000000000
`define _INF 32'b01111111100000000000000000000000
`define MINF 32'b11111111100000000000000000000000
`define _ONE 32'b00111111100000000000000000000000
`define MONE 32'b10111111100000000000000000000000

`define get_exp(x)  (((x)<<1)>>24)
`define get_frac(x)  ((((x)<<9)>>1)|(1<<31))

`define logical_shift(x,shift) ((shift[31])?(x)>>~(shift)+1:(x)<<(shift))





module int2float(input[31:0]xi,output[31:0]y);
	reg sign;
	reg[31:0]x;
	reg[7:0]exp;
	reg[22:0]frac;
	assign y={sign,exp,frac};
	integer i;
	reg[31:0]high;
	always@(*)begin
		if(xi[31])begin
			sign=1;
			x=~xi+1;
		end else begin
			sign=0;
			x=xi;
		end
		high=-127;
		for(i=0;i<32;i++)begin
			if(`get_bit(x,i))begin
				high=i;
			end
		end
		exp=127+high;
		frac=(x<<(31-high+1))>>9;
	end
endmodule



module cond_neg(input[31:0]x,output sign,output[30:0]y);
	assign sign=x[31];
	wire[31:0]neg;
	assign neg=-x;
	assign y=x[31]?neg[30:0]:x[30:0];
endmodule

module priority_encoder(input[30:0]x,output[7:0]_idx,output[22:0]_y);
	reg[22:0]y;
	assign _y=y;
	reg[7:0]idx;
	assign _idx=idx;
	integer i;
	always@(*)begin
		idx=-127;
		for(i=0;i<31;i++)begin
			if(x[i])begin
				idx=i;
			end
		end
		if(idx>30) y=0;
		else y=(x<<(30-idx+1))>>8;
		// for(i=0;i<23;i++)begin
			// y[]=x[];
		// end
	end
endmodule

module add_127(input[7:0]x,output[7:0]y);
	assign y=x+127;
endmodule

module int2float_2(input[31:0]x,output[31:0]y);
	wire[30:0]w1;
	wire[7:0]w2;
	cond_neg cn(x,y[31],w1);
	priority_encoder pe(w1,w2,y[22:0]);
	add_127 a127(w2,y[30:23]);
endmodule


module float2int(input[31:0]x,output[31:0]_y);
	reg[31:0]exp,frac,y;
	assign _y=y;
	reg[31:0]shift;
	always@(*)begin
		if(x==`_NAN || x==`_INF || x==`MINF) y=0;
		else begin
			y=0;
			exp=`get_exp(x);
			frac=`get_frac(x);
			shift=exp-127-31;
			y=`logical_shift(frac,shift);
			if(`get_bit(x,31)) y=~y+1;
		end
	end
endmodule



module faddsub(input[31:0]a,input[31:0]_b,input add,output[31:0]_c);

	function automatic [31:0]assemble_float(input[31:0]sign,exp,frac);begin:body
			reg b24;
			reg[31:0]rounded;
			assemble_float=0;
			rounded=((frac)>>8)+`get_bit(frac,7);
			b24=`get_bit(rounded,24);
			rounded>>=b24;
			exp+=b24;
			rounded&=~(`get_bit(rounded,23)<<23);
			assemble_float|=sign<<31;
			assemble_float|=exp<<23;
			assemble_float|=rounded;
		end
	endfunction


	reg[31:0]c,aexp,bexp,afrac,bfrac,cfrac,cexp,b;
	reg asign,bsign,csign;
	assign _c=c;
	always@(*)begin
		b=_b;

		c=0;
		aexp=`get_exp(a);
		bexp=`get_exp(b);
		asign=`get_bit(a,31);
		bsign=!add^`get_bit(b,31);
		afrac=`get_frac(a)>>1;
		bfrac=`get_frac(b)>>1;
		cexp=0;
		cfrac=0;
		csign=0;
		if     (a==`_NAN || b==`_NAN) c=`_NAN;
		else if(a==`_INF && b==`MINF) c=add?`_NAN:`_INF;
		else if(a==`MINF && b==`_INF) c=add?`_NAN:`MINF;
		else if(a==`_INF && b==`_INF) c=add?`_INF:`_NAN;
		else if(a==`MINF && b==`MINF) c=add?`MINF:`_NAN;
		else begin 
			if(!add) b=`get_bit(b,31)?b&~(1<<31):b|(1<<31);
			if(a==`_INF || b==`_INF) c=`_INF;
			else if(a==`MINF || b==`MINF) c=`MINF;
			else if(a<<1==0) c=b;
			else if(b<<1==0) c=a;
			else if(asign==bsign)begin
				csign=asign;
				if(aexp==bexp)begin
					cexp=aexp+1;
					cfrac=afrac+bfrac;	
				end else if(aexp>bexp)begin
					cexp=aexp+1;
					cfrac=afrac+`logical_shift(bfrac,-aexp+bexp);
				end else if(aexp<bexp)begin
					cexp=bexp+1;
					cfrac=bfrac+`logical_shift(afrac,-bexp+aexp);
				end
				while(cfrac!=0 && !`get_bit(cfrac,31))begin
					cexp--;
					cfrac<<=1;
				end
				c=assemble_float(csign,cexp,cfrac);
			end else begin
				if(aexp==bexp)begin
					cfrac=(afrac>bfrac)?afrac-bfrac:bfrac-afrac;
					cexp=cfrac?aexp+1:0;
					csign=((afrac<bfrac)!=asign);
				end else if(aexp>bexp)begin
					cexp=aexp+1;
					cfrac=afrac-`logical_shift(bfrac,-aexp+bexp);
					csign=asign;
				end else if(aexp<bexp)begin
					cexp=bexp+1;
					cfrac=bfrac-`logical_shift(afrac,-bexp+aexp);
					csign=bsign;
				end
				while(cfrac!=0 && !`get_bit(cfrac,31))begin
					cexp--;
					cfrac<<=1;
				end
				c=assemble_float(csign,cexp,cfrac);
			end
		end
	end
endmodule



module main();
	reg[31:0]x;
	wire [31:0]y,z;

	int2float_2 i2f(x,y);

	float2int f2i(y,z);

	reg[31:0]a_int,b_int;
	wire[31:0]a,b;
	wire[31:0]c,c_int;
	reg add;

	

	int2float i2fa(b_int,b);
	int2float i2fb(a_int,a);
	faddsub as(a,b,add,c);
	float2int f2ic(c,c_int);
	integer i,j,k,ntests,fails;
	real r;
	initial begin
		ntests=0;
		fails=0;

		for(i=-9;i<10;i++)begin
			x=i; 
			ntests++;
			#1
			if(x!=z)begin 
				$display("conversion error: x=%d ,z=%d y = %b t=%f",x,z,y,$time);
				fails++;
			end
		end

		add=1;
		for(i=-10;i<10;i++)begin
			for(j=-10;j<10;j++)begin
				for(k=0;k<=1;k++)begin
					add=k;
					a_int=i;
					b_int=j;
					ntests++;
					#1 
					if((add?a_int+b_int:a_int-b_int)!=c_int)begin
						$display("%s error: %d %s %d = %d",
						add?"add":"sub",a_int,add?"+":"-",b_int,c_int);
						$display("%b =real",add?a_int+b_int:a_int-b_int);
						$display("%b =emu",c_int);
						fails++;
					end
				end
			end
		end

		$display("%d tests failed out of %d",fails,ntests);

		#10 $finish;
	end
endmodule







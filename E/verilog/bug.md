```
    wire vec_1 = {{5{a}},{5{b}}, {5{c}}, {5{d}}, {5{e}}};
    wire vec_2 = {5{{a,b,c,d,e}}};
    assign out = ~(vec_1 ^ vec_2);
```
中文括号很隐蔽

```
module top_module (
    input [31:0] a,
    input [31:0] b,
    output [31:0] sum
);//

endmodule

module add1 ( input a, input b, input cin,   output sum, output cout );
	assign sum = a + b + cin;
    assign cout = (a & b) | (sum & cin);
// Full adder module here
    add16 u_1 (a[15:0], b[15:0], 1'b0,sum[15:0], cout);
    add16 u_2 (a[31:16], b[31:16], cout,sum[31:16],,);
    
endmodule
```

```
wire [7:0] out1, [7:0] out2;  // ❌ 错误
```

### 5
```
module top_module( 
    input [254:0] in,
    output [7:0] out );
    integer i;
    integer j;
    always @(*) begin
        for (i = 0; i < 255; i++) begin
            if (in[i] == 1)
                j++;
        end
        out = j;
    end

endmodule
```
## 6
```
module top_module( 
    input [99:0] a, b,
    input cin,
    output [99:0] cout,
    output [99:0] sum );
    integer i;
    always @(*) begin
        fadd u_1 (
            .a (a[0]),
            .b (b[0]),
            .cin (cin),
            .cout (cout[0]),
            .sum (sum[0])
        );
        for (i = 1; i < 100; i = i + 1) begin
            fadd u_2 (
                .a (a[i]),
                .b (b[i]),
                .cin (cout[i-1]),
                .cout (cout[i]),
                .sum (sum[i])
            );
        end
    end
endmodule



```
## 7
```
module top_module( 
    input [399:0] a, b,
    input cin,
    output cout,
    output [399:0] sum );
	genvar i;
    wire [99:0] cout_j;
    integer j;
    j = 0;
    bcd_fadd u_2(
        .a (a[3:0]),
        .b (b[3:0]),
        .cin (cin),
        .cout (count_j[0]),
        .sum (sum[3:0])
    );
                
    generate
        for ( i = 4;i < 397; i = i + 4) begin: bcd
            bcd_fadd u_2(
                .a (a[i+3:i]),
                .b (b[i+3:i]),
                .cin (cout_j[j]),
                .cout (cout_j[j+1]),
                .sum (sum[i+3:i])
            );
            j = j + 1;
        end
    endgenerate
    cout = count_j[99];
                
endmodule


```
运算符	名称	作用
!	逻辑非（Logical NOT）	判断整体是否为 0，输出 1 位
~	按位取反（Bitwise NOT）	每一位分别取反



```
module top_module (
    input clk,
    input reset,
    input [31:0] in,
    output [31:0] out
);
	genvar i;
    generate
        for (i = 0; i < 32; i = i + 1) begin: capture
            cap u_cap(clk, reset, in[i], out[i]);
        end
    endgenerate
                   
endmodule
module cap(
    input clk,
    input reset,
    input in,
    output out
);
    reg d_in, d_out, clk_in;
    always @(posedge clk) begin
        d_in <= in;
        d_out <= d_in & ~in;
        clk_in = (clk & d_out) | (clk & reset);
        if (clk_in)
            out <= reset ? 1'b0 : d_out;
    end
endmodule
```
![alt text](<../../images/E/屏幕截图 2026-11-04 202948.png>)
![alt text](<../../images/E/屏幕截图 2026-11-04 202954.png>)
![alt text](<../../images/E/屏幕截图 2026-11-04 203154.png>)



```
  assign OneHertz = (q1 == 4b'1001 & q2 == 4'b1001 & q3 == 4'b1001);
```
4'b1001
注意b和'的位置



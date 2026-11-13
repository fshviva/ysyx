# HDLBits 串口接收器｜Fsm_serial + Fsm_serialdp 完整调试复盘

HDLBits 大类: Sequential Logic
HDLBits 小节: Finite State Machines
复盘状态: 已讲解
来源: HDLBits
细分题型: 仿真排错
表现: 逻辑隐患
遇到日期: 2026年11月10日
错误类型: 时序与边沿

这篇笔记合并了本次对话中的两道 HDLBits 题：**Fsm_serial（串口接收，不带校验）** 和 **Fsm_serialdp（串口接收 + 奇校验 + 数据输出）**。按实际代码版本记录所有出现过的错误和相关误区，最后放两道题的完整参考答案。本文**不包含自测题**；部分代码版本曾报告通过，但不代表所有中间版本都经过独立验证。

## 一、题目到底要做什么？

串口每拍接收 1 位，线路空闲时为 1。

| 状态 | 作用 | 正常转移 |
| --- | --- | --- |
| IDLE | 等待起始位 `in=0` | → DATA |
| DATA | 接收 8 个数据位（LSB first） | → STOP（无校验）／PARITY（带校验） |
| PARITY / CHECK | 接收第 9 个比特，即奇校验位 | → STOP |
| STOP | 检查停止位是否为 1 | 有效 → IDLE；无效 → WAIT |
| WAIT | 停止位异常，等待线路恢复为 1 | → IDLE |

`done` 是“这一帧接收成功”的**单周期脉冲**。带奇校验时，停止位和奇校验必须都正确。数据寄存器使用 `data <= {in, data[7:1]};`：第一位先收到的是 bit0，连续移入 8 次后，bit0 恰好被移到 `data[0]`。

## 二、本次对话中确实出现的错误

### A. 语法、模块、信号驱动

**1. 漏掉分号，以及输入了错误的单引号。**

最初写过 `reg [7:0] data`（缺少 `;`）；另有 `count + 1‘b1` 使用了排版弯引号 `‘`，Verilog 数值常量应使用 ASCII 单引号，如 `1'b1`。这类错误属于语法错误，与 FSM 功能无关。

**2. 把 `generate` 放进 `always @(*)`。**

曾将 `generate for` 写在组合过程块里面。`generate` 用于 elaboration（硬件生成/展开），应放在模块作用域，不是 `always` 内执行的过程循环。

**3. 用 `generate for` 实例化 8 个 `parity`，却把输出全部接到同一个 `odd`。**

`generate` 是复制 **8 套真实硬件**，不是让单套硬件运行 8 拍。同一根 `odd` 被多个实例共同驱动，触发了实际报错：

```
Error (12014): Net "odd" ... cannot be assigned more than one value
Error (12015): Net is fed by "parity:u_parity|odd"
Error (12015): Net is fed by "odd"
```

本题只需**一个 parity 模块**，连续统计 8 位数据和 1 位校验位。

**4. 实例输出已经驱动 `odd`，顶层又写 `odd <= 0;`。**

写过 `parity u_parity(clk, res, in, odd);`，同时在顶层 `always @(posedge clk)` 的复位分支给 `odd` 赋值。这样 `odd` 同样有两个驱动源。`odd` 应由 parity 子模块独占驱动；顶层用 `wire odd;` 接收子模块输出，并通过子模块的 `reset` 端口复位。

**5. `reg` / `wire` 的使用位置混淆。**

`done` 曾声明为 `output done`，但在顶层时序 `always` 内执行 `done <= ...`；标准 Verilog 需声明为 `output reg done`。`odd` 则相反：它作为子模块输出的连接线，应声明为 `wire odd`，顶层不要再过程赋值。HDLBits 已提供 parity 模块时也不要重复定义同名模块。

### B. FSM、数据位和校验位时序

**6. 早期没有 `IDLE` 状态，直接从 DATA 开始。**

无起始位等待状态，可能把起始位当作数据位接收。串口空闲为 1；只有在 IDLE 检测到 `in=0` 后，后续的 8 个时钟才对应 8 位数据。

**7. 漏掉 `state <= next_state;`。**

虽然组合块计算了 `next_state`，但早期版本时序块没有真正把它装入 `state`。复位后会一直停在 IDLE。组合逻辑只“算出下一状态”；**只有时钟沿更新 `state` 才是真的状态转移**。

**8. `count` 只有 3 位，却写 `count==8`。**

`reg [2:0] count` 只能表示 0～7，永远到不了 8。后来改成 4 位虽然能数到 8，但如果从 0 开始接收，`count==8` 会**多接收一拍**，把校验位或停止位当成数据。第 8 个数据位的计数值是 **7**：

```verilog
DATA: next_state = (count == 3'd7) ? PARITY : DATA;
```

不带校验的题则把 `PARITY` 改为 `STOP`。

**9. 复位时没有给 `count` 赋初值。**

有版本只写了 `state <= IDLE;`，却没有 `count <= 0;`。这样复位期间计数器可能保存旧值或未知值。规范的同步复位应该同时处理状态、计数器和 `done`；`data` 如需确定的复位输出，也可以初始化为 0。

**10. `res` 当作 parity 复位信号时，逻辑写反了。**

曾写：

```verilog
res = (state == DATA) ? 1 : 0;
parity u_parity(clk, res, in, odd);
```

parity 模块第二个端口是**同步 reset，不是 enable**！DATA 期间 `res=1` 表示每一拍清零，根本无法累计奇偶性。后来改为 DATA 期间 `res=0` 虽然避开这个错误，但若 CHECK 期间又拉高 `res`，仍会把刚收到的数据校验结果清掉。

更稳妥的逻辑：`parity_reset = reset || (state == IDLE);`。在 IDLE 清零，DATA 和 PARITY 期间继续累计。

**11. 某些版本没有把顶层 `reset` 送入 parity 模块的复位条件。**

只依赖 `state==IDLE` 时，若外部复位从 DATA 阶段突然到来，parity 寄存器未必立即在该沿清零。应将外部 `reset` 与空闲复位条件合并。

**12. 在 CHECK 检查完校验位后，直接返回 DATA。**

曾写 `CHECK: next_state = (in == ~odd) ? DATA : WAIT;`。即便奇校验正确也不能直接回 DATA：**后面还有一个停止位**。应确保先接收/判断校验位，再单独采样 STOP；一个成功帧结束后回 IDLE，才能等待下一帧起始位。

**13. WAIT 用奇偶校验结果判断，而不是等待线路恢复。**

早期曾写 `WAIT: next_state = (in == ~odd) ? DATA : WAIT;`。WAIT 不是再次比较 parity；它用于异常停止位后的同步恢复，正常条件应为“`in==1` 后返回 IDLE”。

**14. 将 `odd_2` 写成组合自反馈保持。**

```verilog
assign odd_2 = (state == CHECK) ? odd_1 : odd_2;
```

这不是正规的时钟寄存器，而是组合反馈/锁存器式行为；没有明确的时钟更新和初始化。更清晰的做法是**单个 parity 模块在 PARITY 拍把校验位也累计进去**，并在 STOP 读取 `odd==1`，不必另设 `odd_2`。

**15. 两种奇偶校验方案被混用了。**

下面两种方案各自都可以成立，但需要统一：

- **方案 A（本页参考答案）**：DATA 累计 8 位；PARITY 再累计校验位；STOP 判断 `in && odd`。
- **方案 B**：在 CHECK 用 `in == ~odd` 直接比较校验位（此时 odd 必须只含 8 位数据的统计结果），并把“校验通过”保存在正常的时钟寄存器中；STOP 再使用保存的结果。

`in == ~odd` **本身并不是错误**；错在不区分何时累计、何时清零、何时判断，以及后面是否还有 STOP。

### C. done 输出、复位、组合默认值

**16. `done` 写了一个永远不会满足的条件。**

某版写 `done <= (state == CHECK & next_state == IDLE);`，但当时 CHECK 的转移只有 DATA 或 WAIT，根本没有 CHECK→IDLE，因此 `done` 永远不会置 1。写输出表达式前要看状态图是否存在对应的边。

**17. `done` 放在 `else` 内但复位分支没有赋值。**

这正是后来反复比较的两种写法：

```verilog
// 写法 A：每个 posedge 都给 done 赋值，包括 reset=1
done <= condition;
if (reset)
    state <= IDLE;
else
    state <= next_state;

// 写法 B：仅 reset=0 时更新 done，reset=1 时保持旧值
if (reset)
    state <= IDLE;
else begin
    state <= next_state;
    done <= condition;
end
```

**结论：放在 else 内不等于错误。** `reset=0` 时两者对 `done` 的效果相同；`reset=1` 时，A 继续按条件更新，B 保持旧值，**两者都不保证复位清零**。更规范的是在 `if (reset)` 中明确写 `done <= 1'b0;`，在 `else` 中产生脉冲。

无奇校验版把 `done` 放在 else 内也通过测试，恰好说明“必须放在 if-else 前面才能通过”这个早先解释并不正确；真正需要检查的是复位行为及其它逻辑差异。

**18. 搞混了非阻塞赋值的更新时机。**

```verilog
state <= next_state;
done  <= (state == STOP) && in;
```

同一个时钟沿，两条 `<=` 的右侧都读**该沿更新前**的值；将这两条不同寄存器的赋值语句对调，结果不变。STOP 的停止位为 1 时，在该沿以后 `state` 变 IDLE，`done` 变 1；下一个沿再变 0，得到一拍脉冲。

带奇校验时 STOP 沿本身也会让 parity 模块看到停止位；但由于 `done <= ... && odd` 读取的是**沿前**的 `odd`，此时读到的恰好是 DATA + PARITY 累计结果，停止位不会提前干扰这一拍的判定。

**19. `always @(*)` 中没有对所有路径赋值，存在锁存器风险。**

多版代码写了 `case(state)` 但没有 `default` 或先给 `next_state` 默认值。只要状态进入未列出的编码，`next_state` 就可能需要保持旧值，从而推断出 latch 或产生不确定行为。参考写法：

```verilog
always @(*) begin
    next_state = state;    // 默认：保持当前状态
    case (state)
        IDLE: if (!in) next_state = DATA;
        // 其他状态...
        default: next_state = IDLE;
    endcase
end
```

注意：**没写 `next_state = state;` 不一定就是错误**。你原来的完整三目赋值如 `IDLE: next_state = in ? IDLE : DATA;` 是正确的；只要每个可能输入路径都有赋值，并且妥善处理非法状态，就不需要额外写这句。它是一种便于避免漏赋值的习惯。

## 三、容易混淆但不是这次的错误

- `reg [3:0] count` 本身没错，只是接收 8 位时用 3 位已经够了；真正的问题是 `count==8` 的边界。
- `CHECK` 状态使用 `in == ~odd` 本身有正确的设计前提，不应机械地说这一行一定错。
- 在组合 `always @(*)` 中使用阻塞赋值 `=` 计算 `next_state` 是正常写法；在时钟 `always @(posedge clk)` 中通常使用非阻塞赋值 `<=` 存储状态。
- `&` 和 `&&` 对 1 位布尔条件一般可得到相同的二值结果；多个逻辑条件建议使用 `&&`，使含义清楚。
- `done` 是当前帧成功脉冲，`out_byte` 只要求在 `done==1` 时有效；非接收成功期间的数据值可按题目约定处理。

相关资料：[Finite State Machines｜FSM 状态编码、转移与输出（待学习）](https://app.notion.com/p/Finite-State-Machines-FSM-411b7f380238815aa6cce1b4e5696198?pvs=21)、[非阻塞赋值 <=：读旧值、后赋值覆盖与 if/else 归属](https://app.notion.com/p/if-else-411b7f38023881d09571f5607b1445b9?pvs=21)。

---

## 四、标准答案（参考实现）

下列是**与 HDLBits 题意等价的规范参考实现**，不代表官网唯一代码。统一采用“组合 next_state + 时序 state/数据/输出”的结构，并在每个同步复位分支显式清零 `done`。

### 1. Fsm_serial：无奇偶校验，只输出 done

```verilog
module top_module(
    input clk,
    input in,
    input reset,           // Synchronous reset
    output reg done
);

    parameter IDLE = 2'd0,
              DATA = 2'd1,
              STOP = 2'd2,
              WAIT = 2'd3;

    reg [1:0] state, next_state;
    reg [2:0] count;

    // 下一状态组合逻辑
    always @(*) begin
        next_state = state;
        case (state)
            IDLE: if (!in) next_state = DATA;
            DATA: if (count == 3'd7) next_state = STOP;
            STOP: next_state = in ? IDLE : WAIT;
            WAIT: if (in) next_state = IDLE;
            default: next_state = IDLE;
        endcase
    end

    // 状态、计数、完成脉冲
    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            count <= 3'd0;
            done  <= 1'b0;
        end else begin
            state <= next_state;
            if (state == DATA)
                count <= count + 3'd1;
            else
                count <= 3'd0;

            done <= (state == STOP) && in;
        end
    end

endmodule
```

### 2. Fsm_serialdp：8 位数据 + 奇校验 + 停止位

本题 HDLBits 已提供 `parity` 模块；**只实例化一次**，无需在提交代码中再次定义 parity。

```verilog
module top_module(
    input clk,
    input in,
    input reset,          // Synchronous reset
    output [7:0] out_byte,
    output reg done
);

    parameter IDLE   = 3'd0,
              DATA   = 3'd1,
              PARITY = 3'd2,
              STOP   = 3'd3,
              WAIT   = 3'd4;

    reg [2:0] state, next_state;
    reg [2:0] count;
    reg [7:0] data;

    wire odd;
    wire parity_reset;
    assign parity_reset = reset || (state == IDLE);

    parity u_parity (
        .clk(clk),
        .reset(parity_reset),
        .in(in),
        .odd(odd)
    );

    // 下一状态组合逻辑
    always @(*) begin
        next_state = state;
        case (state)
            IDLE:   if (!in) next_state = DATA;
            DATA:   if (count == 3'd7) next_state = PARITY;
            PARITY: next_state = STOP;
            STOP:   next_state = in ? IDLE : WAIT;
            WAIT:   if (in) next_state = IDLE;
            default: next_state = IDLE;
        endcase
    end

    // 时序状态机与数据通路
    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            count <= 3'd0;
            data  <= 8'd0;
            done  <= 1'b0;
        end else begin
            state <= next_state;

            if (state == DATA) begin
                count <= count + 3'd1;
                data  <= {in, data[7:1]};
            end else begin
                count <= 3'd0;
            end

            // odd 在 STOP 采样沿前表示 8 数据位+校验位的奇偶
            done <= (state == STOP) && in && odd;
        end
    end

    assign out_byte = data;

endmodule
```

---

## 五、补充题：低有效异步复位 FSM 中，z 为什么不能放在 if-else 外？

这是随后讨论的另一道 HDLBits FSM 题（`aresetn` 为低电平有效的**异步复位**）。与前面串口题的同步复位对比，更容易理解“输出赋值放在 `else` 内外”到底改变什么。

### 1. 用户原代码中的关键错误

```verilog
always @(posedge clk or posedge aresetn) begin
    if (~aresetn) begin
        state <= a;
        z <= 0;
    end
    else
        state <= next_state;
    z <= (state == c) & x;
end
```

**问题 ①：复位边沿写反。** `aresetn` 的 n 表示低有效，应该在 `aresetn` **1→0** 时立即复位：`always @(posedge clk or negedge aresetn)`，而不是 `posedge aresetn`。复位判断可用 `if (!aresetn)`。

**问题 ②：`else` 只管紧随其后的一条语句。** 没有 `begin/end` 时，只有 `state <= next_state;` 属于 `else`。后面的 `z <= (state == c) & x;` 无条件执行。

**问题 ③：复位路径对 `z` 重复安排赋值。** `if (!aresetn)` 成立时先有 `z <= 0;`，又执行后面的 `z <= (state == c) & x;`。在同一个时序过程的同一次执行中，后执行的非阻塞赋值会覆盖之前给同一寄存器安排的赋值。因此 `z` 的复位为 0 不一定生效。

**问题 ④：异步复位事件不应装入任意数据。** 修正为 `negedge aresetn` 后，若 `z <= (state==c)&x` 仍在 `if-else` 外，复位下降沿也会要求 `z` 采样由 `state` 与 `x` 决定的普通数据。普通异步清零 DFF 只能在复位沿强制固定值，不能把复位沿当成第二个数据采样时钟；可能导致无法综合成期望的寄存器结构。这里**没有提供本题实际 Quartus 报错文本**，不要把另一道题的 10237 警告照搬成这次的日志。

**问题 ⑤：`output z` 默认是 wire，却在 always 中赋值。** 标准 Verilog 需要把过程赋值输出声明成 `output reg z`；但本题的 `z` 更适合直接连续赋值 `assign`，因此保留 `output z` 即可。

**问题 ⑥：组合状态转移没有覆盖非法编码。** `reg [1:0] state` 还能取 `2'b11`，但 `case` 没有 `default`，可能推断锁存器。可以在进入 `case` 前先默认赋值，也可以写 `default: next_state = a;`。

### 2. 和串口题 done 放在 else 内有什么区别？

| 情况 | 同步复位（串口 done） | 异步复位（本题 z） |
| --- | --- | --- |
| always 触发事件 | 只有 `posedge clk` | `posedge clk` 或 `negedge aresetn` |
| 赋值放在 if-else 外 | 仍只在时钟沿赋值，但可能不符合复位要求 | 复位沿也可能执行普通数据赋值，造成综合和复位问题 |
| 推荐设计 | 复位分支明确 `done <= 0` | 状态寄存器异步复位；`z` 作为组合输出单独 `assign` |

**重点：不是规定所有输出必须写进 else。** 要看输出是组合逻辑还是时序寄存器，也要看 `always` 的敏感列表和复位要求。

### 3. 这道题的参考答案

```verilog
module top_module (
    input clk,
    input aresetn,     // Asynchronous active-low reset
    input x,
    output z
);
    parameter a = 2'b00,
              b = 2'b01,
              c = 2'b10;

    reg [1:0] state, next_state;

    // 下一状态组合逻辑
    always @(*) begin
        next_state = a;
        case (state)
            a: next_state = x ? b : a;
            b: next_state = x ? c : a;
            c: next_state = a;
            default: next_state = a;
        endcase
    end

    // 状态寄存器：低有效异步复位
    always @(posedge clk or negedge aresetn) begin
        if (!aresetn)
            state <= a;
        else
            state <= next_state;
    end

    // Mealy 输出（组合逻辑）
    assign z = (state == c) && x;
endmodule
```

相关旧案例：[Quartus 10237｜异步复位 always 中的 aaah 为什么放进 else 才能综合](https://app.notion.com/p/Quartus-10237-always-aaah-else-413b7f3802388142bf31e4df934077c1?pvs=21)。
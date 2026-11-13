<!-- embedded-architecture:begin -->
## 嵌入式代码实施规则

本工程的嵌入式移植、新增功能、修复和重构，必须遵循
[工程代码规则](docs/embedded_rules/code_rules.md)及其中链接的四份规范。
修改前先读相关规范；新增/修改涉及的.h公共接口使用中文Doxygen；
所有FSM错误终态必须记录具体可读错误码、底层原因和文件/函数/行号。
依照规范完成实际代码、构建与验证，不能以检查报告代替实现。
已有用户明确约定和更高优先级指令仍优先；保留本文件其他内容。
<!-- embedded-architecture:end -->

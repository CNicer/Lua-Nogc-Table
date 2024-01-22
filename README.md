# Lua-Nogc-Table


[English Document](#2)

<p id="1"></p>

## 功能

将Lua表从gc的各个流程中剔除，并拒绝该表的修改和添加。
可用于减少GC消耗，例如Lua配置表过大时，这种表一般常驻且只读，可调用该nogc函数。

## 使用说明



# English Document

## ability

Set the table not to participate in gc to reduce gc time-consuming
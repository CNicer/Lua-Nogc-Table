# Lua-Nogc-Table


[English Document](#2)

<p id="1"></p>

## 功能

将Lua表从gc的各个流程中剔除，并拒绝该表的修改和添加。<br>
可用于减少GC消耗，例如Lua配置表过大时，这种表一般常驻且只读，可调用该nogc函数。

## 目录

luasrc文件夹下存放完整的Lua源文件（版本5.4.4）<br>
test文件夹下存放简单示例

## 使用

开发和测试基于5.4.4版本。<br><br>
其他lua版本使用时，按以下步骤
1. 在luasrc文件夹中的xnogc.cpp和xnogc.h放到lua源文件目录下
2. 将lgc.c中的traversetable函数用下面的函数替代
```c
/* 添加了一个skiptable的判断 */
static lu_mem traversetable (global_State *g, Table *h) {
  const char *weakkey, *weakvalue, *skiptable;
  const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
  markobjectN(g, h->metatable);
  if (mode && ttisstring(mode) &&  /* is there a weak mode? */
      (cast_void(weakkey = strchr(svalue(mode), 'k')),
       cast_void(weakvalue = strchr(svalue(mode), 'v')),
       cast_void(skiptable = strchr(svalue(mode), 's')),
       (weakkey || weakvalue || skiptable))) {  /* is really weak? */ /* is skip table? */
    if (skiptable) goto END;
    if (!weakkey)  /* strong keys? */
      traverseweakvalue(g, h);
    else if (!weakvalue)  /* strong values? */
      traverseephemeron(g, h, 0);
    else  /* all weak */
      linkgclist(h, g->allweak);  /* nothing to traverse now */
  }
  else  /* not weak */
    traversestrongtable(g, h);
  END:
  return 1 + h->alimit + 2 * allocsizenode(h);
}
```
3. 如果希望表同时为只读，不允许修改插入，按以下修改

## lua使用实例




# English Document

## ability

Set the table not to participate in gc to reduce gc time-consuming
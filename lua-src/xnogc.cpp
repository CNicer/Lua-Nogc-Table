#include "xnogc.h"

extern "C" {
#include "lstate.h"
#include "ltable.h"
#include "lauxlib.h"
#include "lstring.h"
#include "lvm.h"
}


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unordered_set>
#include <time.h>

#ifdef __linux__
#define likely(x) __builtin_expect(!!(x), 1) 
#define unlikely(x) __builtin_expect(!!(x), 0)
#elif _WIN64
#define likeyly(x) x
#define unlikely(x) x
#endif

#define TRAVERSE_CASE case LUA_VTABLE: case LUA_VLCL: case LUA_VPROTO:
#define NOTRAVERSE_CASE case LUA_VSHRSTR: case LUA_VLNGSTR: case LUA_VUSERDATA: case LUA_VTHREAD:

#define C2MS(e, s) (e-s)*1000/CLOCKS_PER_SEC
#define COUNTCOST(c, n) \
c;
//clock_t s = clock(); c; clock_t e = clock(); printf(#n " cost %d\n", C2MS(e,s));

#define GETPMIN(p) if(reinterpret_cast<void*>(p) < pmin) pmin = p;
#define GETPMAX(p) if(reinterpret_cast<void*>(p) > pmax) pmax = p;
#define GETM(p) GETPMIN(p)GETPMAX(p)

#define insertN(o ,s) if(o) s.insert(obj2gco(o));GETM(o)
#define insertS(o, s) s.insert(obj2gco(o));GETM(o)

using gcset = std::unordered_set<GCObject*>;

//#define DEBUG 1

void* pmax = 0;
void* pmin = reinterpret_cast<void*>(0xFFFFFFFFFFFFFFFF);

// only support table type
static Table* luaX_gettable(lua_State* L, int idx)
{
	TValue* t;
	CallInfo* ci = L->ci;
	if (idx > 0) {
		StkId o = ci->func + idx;
		api_check(L, idx <= L->ci->top - (ci->func + 1), "unacceptable index");
		if (o >= L->top) t = &G(L)->nilvalue;
		else t = s2v(o);
	}
	else {
		return NULL;
	}

	api_check(L, ttistable(t), "table expected");
	return hvalue(t);
}

static void traverse(GCObject* o, gcset& allgc_h);

static void traversetable(Table* h, gcset& allgc_h)
{
	Node* n, * limit = &h->node[(size_t)(1<<(h->lsizenode))];
	unsigned int i, asize = luaH_realasize(h);
	GCObject* subo = NULL;
	for (i = 0; i < asize; i++)
	{
		subo = gcvalue(&h->array[i]);
		if (subo->tt < LUA_TSTRING) continue;	/* skip gcobject by count */
		switch (subo->tt)
		{
		TRAVERSE_CASE traverse(subo, allgc_h);
		NOTRAVERSE_CASE insertS(subo, allgc_h)
		}
	}
	for (n = gnode(h, 0); n < limit; n++)
	{
		if (isempty(gval(n))) continue;
		if (((gval(n))->tt_) < LUA_TSTRING) continue;	/* skip gcobject by count */
		subo = gcvalue(gval(n));
		switch (subo->tt)
		{
		TRAVERSE_CASE traverse(subo, allgc_h);
		NOTRAVERSE_CASE insertS(subo, allgc_h)
		}
	}
}

static void traverseproto(Proto* f, gcset& allgc_h) {
	int i;
	insertN(f->source, allgc_h);
	for (i = 0; i < f->sizek; i++)  /* mark literals */
	{
		TValue* v = &f->k[i];
		if (v->tt_ < LUA_TSTRING) continue;
		GCObject* o = gcvalue(v);
		switch (o->tt)
		{
			TRAVERSE_CASE traverse(o, allgc_h);
			NOTRAVERSE_CASE insertS(o, allgc_h)
		}
	}
	for (i = 0; i < f->sizeupvalues; i++)  /* mark upvalue names */
	{
		insertN(f->upvalues[i].name, allgc_h);
	}
	for (i = 0; i < f->sizep; i++)  /* mark nested protos */
	{
		Proto* p = f->p[i];
		if (p)
		{
			insertS(p, allgc_h)
			traverseproto(p, allgc_h);
		}	
	}
	for (i = 0; i < f->sizelocvars; i++)  /* mark local-variable names */
	{
		insertN(f->locvars[i].varname, allgc_h);
	}
}

static void traverseLclosure(LClosure *cl, gcset& allgc_h)
{
	insertN(cl->p, allgc_h);
	if (cl->p)
	{
		insertS(cl->p, allgc_h)
		traverseproto(cl->p, allgc_h);
	}
	for (int i = 0; i < cl->nupvalues; i++) {
		UpVal* uv = cl->upvals[i];
		insertN(uv, allgc_h);
	}
}

static void inline traverse(GCObject* o, gcset& allgc_h)
{
	switch (o->tt) {
	case LUA_VTABLE: traversetable(gco2t(o), allgc_h); break;
	case LUA_VLCL: traverseLclosure(gco2lcl(o), allgc_h); break;
	case LUA_VPROTO: traverseproto(gco2p(o), allgc_h); break;
	default:break;
	}
}

/* default: table is the first stackvalue */
static bool setTskip(lua_State* L, Table* t)
{
	if (unlikely(lua_getmetatable(L, 1)))
	{
		lua_getfield(L, 2, "__mode");

		if (likeyly(!lua_isstring(L, -1)))
		{
			lua_pop(L, 1);
			lua_pushstring(L, "s");
		}
		else
		{
			const char* mode = lua_tostring(L, -1);
			if (strchr(mode, 's')) return false; /* no repeate */

			/* tv is no meaning when set table to s */
			/*
			char* smode = (char*)(malloc(sizeof(char) * (strlen(mode) + 2)));
			strcpy_s(smode, strlen(mode), mode);
			strcat_s(smode, 1, "s");
			lua_pushstring(L, smode);
			free(smode);
			*/

			lua_pushstring(L, "s");
		}
		lua_setfield(L, 2, "__mode");
	}
	else
	{
		//lua_lock(L);
		//Table* mt = luaH_new(L);

		//TString* svalue = luaS_new(L, "s");
		//TString* skey = luaS_new(L, "__mode");

		//const TValue* slot;

		//TValue* vtable = new TValue;
		//vtable->value_.gc = obj2gco(mt);
		//vtable->tt_ = ctb(mt->tt);

		//luaV_fastget(L, vtable, skey, slot, luaH_getstr);

		//TValue* vvalue = new TValue;
		//TValue* vkey = new TValue;
		//vvalue->value_.gc = obj2gco(svalue);
		//vvalue->tt_ = ctb(svalue->tt);
		//vkey->value_.gc = obj2gco(skey);
		//vkey->tt_ = ctb(skey->tt);

		//luaV_finishset(L, vtable, vvalue, vkey, slot);

		//t->metatable = mt;

		//vtable->value_.gc = vvalue->value_.gc = vkey->value_.gc = nullptr;
		//delete vtable, vvalue, vkey;

		//lua_unlock(L);

		lua_newtable(L);
		lua_pushstring(L, "s");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, 1);
	}
	return true;
}

static void removefromallgc(lua_State* L, gcset& allgc_h)
{
	global_State* g = G(L);
	GCObject* prep = nullptr, * p = g->allgc;

	while (p)
	{
		if (p >= pmin && p <= pmax && allgc_h.find(p) != allgc_h.end())
		{
			if (prep) prep->next = p->next;
			else g->allgc = p->next;
		}
		else
		{
			prep = p;
		}

		p = p->next;
	}
}


static int luaX_nogc(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	Table* t = luaX_gettable(L, 1);

	/* set __mode="s"*/
	if (!setTskip(L, t)) return 0;

	gcset allgc_h;
	allgc_h.insert(obj2gco(t));

	COUNTCOST(traversetable(t, allgc_h), traverse)

	removefromallgc(L, allgc_h);

#ifdef DEBUG
	p = g->allgc;
	while (p)
	{
		if (allgc_h.find(p) != allgc_h.end())
		{
			printf("0x%llx is still in allgc\n", (uint64_t)p);
		}
		p = p->next;
	}
#endif
	return 0;
}

static gcset awaitgcoset;

static int luaX_nogcreserve(lua_State* L) {
	luaL_checkinteger(L, 1);

	uint32_t reserven = lua_tonumber(L, 1);
	awaitgcoset.reserve(reserven);
	return 0;
}

/* firenogc must be called after calling awaitnogc */
static int luaX_awaitnogc(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	Table* t = luaX_gettable(L, 1);
	t->gclist = reinterpret_cast<GCObject*>(0xFFFFFFFFFFFFFFFF);
	if (!setTskip(L, t)) return 0;
	COUNTCOST(traversetable(t, awaitgcoset), traverse)

	return 0;
}

static int luaX_firenogc(lua_State* L) {
	lua_pushnumber(L, awaitgcoset.size());
	COUNTCOST(removefromallgc(L, awaitgcoset), remove)
	awaitgcoset.clear();
	return 1;
}

static const luaL_Reg basenogc_funcs[] = {
	{"nogc", luaX_nogc},
	{"nogcreserve", luaX_nogcreserve},
	{"awaitnogc", luaX_awaitnogc},
	{"firenogc", luaX_firenogc},
	{NULL, NULL}
};

extern int luaL_opengclibs(lua_State* L) {
	lua_pushglobaltable(L);
	luaL_setfuncs(L, basenogc_funcs, 0);
	return 0;
}
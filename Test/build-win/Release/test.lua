local t = {
    func = function(a)
        print("func alive a=".. a)
    end
}

for i = 1, 50000 do
    t[i] = {a=i, b="hello"..i}
    t[i.."_name"] = {c = i+10000, b = "world"..i}
end

t.func(1)

-- Test of reducing GC time consumption after nogc
print("\n===========nogc后减少的gc耗时测试=================")
function calcost(func, desc)
    local s = get_microseconds()
    func()
    print(desc .. " cost=" .. (get_microseconds() - s) .. "us")
end

for i = 1, 5 do
    calcost(function()
        collectgarbage("collect")
    end, "before nogc collect garbarge cost")
end

calcost(function()
    nogc(t)
end, "nogc")

for i = 1, 5 do
    calcost(function()
        collectgarbage("collect")
    end, "after nogc collect garbarge cost")
end

t.func(1)

-- After nogc, the fields in the table do not participate in the gc test
print("\n\n===========nogc后table中的字段不参与gc测试=================")
local temp = {
    func = t.func
}

temp.func(2)
temp=nil
collectgarbage("collect")
t.func(2)
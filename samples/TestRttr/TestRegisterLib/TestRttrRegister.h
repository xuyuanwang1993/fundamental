#pragma once
#include <string>

enum TestEnumType
{
    TestEnum1,
    TestEnum2,
    TestEnum3,
    TestEnum4
};

struct  TestTypeRegister
{
    F_PLUGIN_API TestTypeRegister();
    std::string name;
    std::string type;
    std::string custom;
    TestEnumType enumType = TestEnum1;
};

struct TestInstance
{
    static int * GetInstance1();
    static int * GetInstance2();
    inline static int x;
};

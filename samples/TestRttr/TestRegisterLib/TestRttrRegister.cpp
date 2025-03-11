#if 1
#include "TestRttrRegister.h"
#include <rttr/registration>
#include <iostream>
#include "fundamental/basic/log.h"
#include "fundamental/rttr_handler/deserializer.h"
struct MyPluginClass
{
    MyPluginClass()
    {
    }

    void perform_calculation()
    {
        value += 12;
    }

    void perform_calculation(int new_value)
    {
        value += new_value;
    }

    int value = 0;
};

RTTR_PLUGIN_REGISTRATION
{
    using namespace rttr;
    {

        using ClassType = TestTypeRegister;
        rttr::registration::class_<ClassType>("TestTypeRegister")
            .constructor()(rttr::policy::ctor::as_object)
            .property("name", &TestTypeRegister::name)
            .property("custom", &TestTypeRegister::custom)
            .property("type", &TestTypeRegister::type)
            .property("enumType", &TestTypeRegister::enumType);
    }
    {
        using ClassType = TestEnumType;
        rttr::registration::enumeration<TestEnumType>("TestEnumType")(
            value("TestEnum1", ClassType::TestEnum1),
            value("TestEnum2", ClassType::TestEnum2),
            value("TestEnum3", ClassType::TestEnum3),
            value("TestEnum4", ClassType::TestEnum4));
    }
    rttr::registration::class_<MyPluginClass>("MyPluginClass")
        .constructor<>()
        .property("value", &MyPluginClass::value)
        .method("perform_calculation", rttr::select_overload<void(void)>(&MyPluginClass::perform_calculation))
        .method("perform_calculation", rttr::select_overload<void(int)>(&MyPluginClass::perform_calculation));
}
void  TestRegisterFunc()
{
    /*rttr_auto_register_reflection_function_();
    rttr_auto_register_reflection_function_();*/
}

TestTypeRegister::TestTypeRegister()
{
}

int * TestInstance::GetInstance1()
{
    static int i;
    return &i;
}
static int instance2;
int * TestInstance::GetInstance2()
{
    return &instance2;
}
DECALRE_PLUGIN_INIT_FUNCTION(PLUGIN_NAME)
{
    //FINFO("{} init", PLUGIN_NAME);
    FWARN("plugin member->{}",(void*)&TestInstance::x);
    FWARN("plugin func->{}",(void*)TestInstance::GetInstance1());
    FWARN("plugin external->{}",(void*)TestInstance::GetInstance2());
    Fundamental::Logger::TestLogInstance();
    Fundamental::TestRttrInstance();
}
#endif

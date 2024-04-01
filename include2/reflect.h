#pragma once

#include <string>
#include <map>
#include "MDApplication.h"

//创建对象的函数指针类型
typedef MDApplication*(*CreateAppObject)(void);

//工厂类，含有类名字符串和创建类实例的函数的映射
class ReflectFactory
{
private:
	ReflectFactory() = default;
	ReflectFactory(const ReflectFactory& ref) = default;
	ReflectFactory& operator=(const ReflectFactory& ref) = default;

public:
	//获得工厂实例
	static ReflectFactory* GetInstance();

	//注册类名和函数的映射
	void RegisterClass(const std::string& className, CreateAppObject func);

	//根据类名获得类对象
	MDApplication* GetAppObject(const std::string& name);

private:
	static ReflectFactory* _singleton;
	std::map<std::string, CreateAppObject> _classAndCreate;
};

//用来自动注册的类，建一个全局变量，自动实现注册
class RegisterAction
{
public:
	RegisterAction(const std::string& name, CreateAppObject func);
	RegisterAction()
	{
		printf("RegisterAction()\n");
	}
};

//定义创建实例的函数，定义RegisterAction全局变量，这些操作写入一个宏里
#define REGISTER(className)\
    MDApplication* objectCreator##className(){\
        return new className();\
    }\
    RegisterAction g_creatorRegister##className(\
        #className,objectCreator##className);


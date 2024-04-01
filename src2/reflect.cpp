#include <iostream>
#include "reflect.h"

using std::string;

ReflectFactory* ReflectFactory::_singleton = NULL;

ReflectFactory* ReflectFactory::GetInstance()
{
	if (_singleton == NULL)
	{
		_singleton = new ReflectFactory();
	}

	return _singleton;
}

void ReflectFactory::RegisterClass(const string& className, CreateAppObject func)
{
	//_classAndCreate[className] = func;
	printf("ReflectFactory::RegisterClass() className = %s\n", className.c_str());
	std::pair<std::map<string, CreateAppObject>::iterator, bool> ret = _classAndCreate.insert(std::pair<std::string, CreateAppObject>(className, func));
	if (ret.second == false)
	{
		printf("ReflectFactory::RegisterClass() failed, class %s has existed\n", className.c_str());
	}
	return;
}

//根据类名获得类对象
MDApplication* ReflectFactory::GetAppObject(const std::string& name)
{
	std::map<std::string, CreateAppObject>::const_iterator iter;
	iter = _classAndCreate.find(name);
	if (iter == _classAndCreate.end())
	{
		printf("ReflectFactory::GetObjectByName: do not exist class %s\n", name.c_str());
		//不存在这个类名
		return NULL;
	}
	else
	{
		//创建对象
		CreateAppObject func = iter->second;
		return func();
	}
}

RegisterAction::RegisterAction(const string& name, CreateAppObject func)
{
	//printf("RegisterAction 构造函数\n");
	ReflectFactory* fact = ReflectFactory::GetInstance();
	fact->RegisterClass(name, func);
}
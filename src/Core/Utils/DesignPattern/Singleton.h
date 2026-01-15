#pragma once

// 模板类：T 是继承这个单例的子类类型
template <typename T>
class Singleton {
public:
    // 获取单例引用的静态方法
    static T& Get() {
        // C++11 保证：静态局部变量的初始化是线程安全的
        // 且只会初始化一次，直到程序结束自动销毁
        static T instance;
        return instance;
    }

    // 禁止拷贝构造 (单例必须唯一)
    Singleton(const Singleton&) = delete;
    // 禁止赋值操作
    Singleton& operator=(const Singleton&) = delete;

protected:
    // 构造函数设为 protected，防止外部直接 new Singleton()
    // 只有子类 (T) 可以调用
    Singleton() = default;
    virtual ~Singleton() = default;
};
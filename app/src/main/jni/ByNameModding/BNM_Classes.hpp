#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring> // for strcmp
#include <memory>

// Forward declare IL2CPP types if not already included
namespace IL2CPP {
    class Class;
    class Object;
    
    namespace Exports {
        extern void* m_IL2CPP_DOMAIN_GET;
        
        // Add missing method declarations
        void* GetClassFromName(const char* nameSpace, const char* className);
    }
    
    void Initialize();
    
    // Class forward declaration
    class Class;
}

#include "Structures/IL2CPP.hpp" // Include after forward declarations

namespace BNM {
    // Forward declarations
    class MethodInfo;
    class FieldInfo;
    class Type;
    class Class;
    
    // InitResolveFunc type
    using InitResolveFunc = void(*)(void*);
    
    // Type class
    class Type {
    public:
        const char* name;
        uint32_t attrs;
        void* data;
        
        Type() : name(nullptr), attrs(0), data(nullptr) {}
        Type(const char* n, uint32_t a = 0, void* d = nullptr) : name(n), attrs(a), data(d) {}
    };
    
    // FieldInfo class
    class FieldInfo {
    public:
        const char* name;
        Type type;
        uint32_t offset;
        uint32_t token;
        
        FieldInfo() : name(nullptr), type(), offset(0), token(0) {}
        FieldInfo(const char* n, const Type& t, uint32_t o, uint32_t tk) 
            : name(n), type(t), offset(o), token(tk) {}
    };
    
    // MethodInfo class
    class MethodInfo {
    public:
        void* methodPointer;
        void* invoker_method;
        const char* name;
        Type returnType;
        std::vector<Type> parameters;
        uint32_t flags;
        uint32_t iflags;
        uint32_t token;
        
        MethodInfo() : methodPointer(nullptr), invoker_method(nullptr), name(nullptr),
                      returnType(), parameters(), flags(0), iflags(0), token(0) {}
        
        // Invoke method with parameters
        template<typename... Args>
        auto Invoke(IL2CPP::Object* obj, Args... args) const {
            using FuncType = decltype(methodPointer);
            return reinterpret_cast<FuncType>(methodPointer)(obj, args...);
        }
        
        // Static invoke
        template<typename... Args>
        auto InvokeStatic(Args... args) const {
            using FuncType = decltype(methodPointer);
            return reinterpret_cast<FuncType>(methodPointer)(args...);
        }
    };
    
    // Class info
    class Class {
    public:
        const char* name;
        const char* namespaze;
        Type type;
        std::vector<FieldInfo> fields;
        std::vector<MethodInfo> methods;
        
        Class() : name(nullptr), namespaze(nullptr) {}
        Class(const char* n, const char* ns) : name(n), namespaze(ns) {}
        
        // Find method by name and parameter count
        MethodInfo* GetMethod(const char* methodName, int paramCount = -1) {
            for (auto& method : methods) {
                if (strcmp(method.name, methodName) == 0 && 
                    (paramCount == -1 || method.parameters.size() == (size_t)paramCount)) {
                    return &method;
                }
            }
            return nullptr;
        }
        
        // Find field by name
        FieldInfo* GetField(const char* fieldName) {
            for (auto& field : fields) {
                if (strcmp(field.name, fieldName) == 0) {
                    return &field;
                }
            }
            return nullptr;
        }
    };
    
    // Main BNM class with static methods
    class BNM {
    public:
        // Initialize BNM with resolve function
        static bool InitResolveFunc(void* address) {
            if (!address) {
                LOGE("BNM: Invalid address provided for initialization");
                return false;
            }
            
            try {
                // Store the address for later use if needed
                (void)address; // Mark as used
                
                // Initialize IL2CPP if not already done
                if (!IL2CPP::Exports::m_IL2CPP_DOMAIN_GET) {
                    LOGI("BNM: Initializing IL2CPP...");
                    IL2CPP::Initialize();
                    
                    // Verify initialization was successful
                    if (!IL2CPP::Exports::m_IL2CPP_DOMAIN_GET) {
                        LOGE("BNM: Failed to initialize IL2CPP");
                        return false;
                    }
                }
                
                LOGI("BNM: Initialized successfully");
                return true;
            } catch (const std::exception& e) {
                LOGE("BNM: Exception during initialization: %s", e.what());
                return false;
            } catch (...) {
                LOGE("BNM: Unknown exception during initialization");
                return false;
            }
        }
        
        // Find class by namespace and name
        static std::shared_ptr<Class> FindClass(const char* nameSpace, const char* className) {
            if (!nameSpace) nameSpace = "";
            if (!className) return nullptr;
            
            auto klass = (IL2CPP::Class*)IL2CPP::Exports::GetClassFromName(nameSpace, className);
            if (!klass) {
                LOGW("BNM: Class %s.%s not found", nameSpace, className);
                return nullptr;
            }
            
            auto result = std::make_shared<Class>(klass->Name(), nameSpace);
            // TODO: Populate fields and methods from klass
            return result;
        }
        
        // Find method by class and method name
        static std::shared_ptr<MethodInfo> FindMethod(const std::shared_ptr<Class>& klass, const char* methodName, int paramCount = -1) {
            if (!klass || !methodName) return nullptr;
            return std::shared_ptr<MethodInfo>(klass->GetMethod(methodName, paramCount));
        }
        
        // Find field by class and field name
        static std::shared_ptr<FieldInfo> FindField(const std::shared_ptr<Class>& klass, const char* fieldName) {
            if (!klass || !fieldName) return nullptr;
            return std::shared_ptr<FieldInfo>(klass->GetField(fieldName));
        }
    };
}

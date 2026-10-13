package com.craftinginterpreters.lox;

import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {
    final String name;
    final LoxClass superclass;
    private final Map<String, LoxFunction> methods;

    LoxClass(String name, LoxClass superclass,
             Map<String, LoxFunction> methods) {
        this.superclass = superclass;
        this.name = name;
        this.methods = methods;
    }

    LoxFunction findMethod(LoxInstance instance, String name) {
        LoxFunction method = null;
        LoxFunction inner = null;
        LoxClass klass = this;
        while (klass != null) {
            if (klass.methods.containsKey(name)) {
                inner = method;
                method = klass.methods.get(name);
            }

            klass = klass.superclass;
        }

        if (method != null) {
            return method.bind(instance, inner);
        }

        return null;
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        LoxInstance instance = new LoxInstance(this);
        LoxFunction initializer = findMethod(instance, "init");
        if (initializer != null) {
            initializer.call(interpreter, arguments);
        }

        return instance;
    }

    @Override
    public int arity() {
        LoxFunction initializer = findMethod("init");
        if (initializer == null) return 0;
        return initializer.arity();
    }
}
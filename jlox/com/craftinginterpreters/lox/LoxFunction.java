package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {
    private final String name;
    private final List<Token> parameters;
    private final List<Stmt> body;
    private final Environment closure;
    private final boolean isInitializer;

    LoxFunction(Stmt.Function declaration, Environment closure,
                boolean isInitializer) {
        this.name = declaration.name.lexeme;
        this.parameters = declaration.params;
        this.body = declaration.body;
        this.isInitializer = isInitializer;
        this.closure = closure;
    }

    LoxFunction(Expr.Function declaration, Environment closure,
                boolean isInitializer) {
        this.name = null;
        this.parameters = declaration.parameters;
        this.body = declaration.body;
        this.isInitializer = isInitializer;
        this.closure = closure;
    }

    LoxFunction bind(LoxInstance instance) {
        Environment environment = new Environment(closure);
        environment.define("this", instance);
        return new LoxFunction(parameters, body, environment,
                isInitializer);
    }

    private LoxFunction(List<Token> parameters, List<Stmt> body,
                        Environment closure, boolean isInitializer) {
        this.name = null;
        this.parameters = parameters;
        this.body = body;
        this.closure = closure;
        this.isInitializer = isInitializer;
    }

    @Override
    public String toString() {
        if (name == null) return "<fn>";
        return "<fn " + name + ">";
    }

    @Override
    public int arity() {
        return parameters.size();
    }

    @Override
    public Object call(Interpreter interpreter,
                       List<Object> arguments) {
        Environment environment = new Environment(closure);
        for (int i = 0; i < parameters.size(); i++) {
            environment.define(parameters.get(i).lexeme,
                    arguments.get(i));
        }

        try {
            interpreter.executeBlock(body, environment);
        } catch (Return returnValue) {
            if (isInitializer) return closure.getAt(0, "this");

            return returnValue.value;
        }

        if (isInitializer) return closure.getAt(0, "this");
        return null;
    }


}

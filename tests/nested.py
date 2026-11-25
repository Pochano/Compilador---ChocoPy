def f() -> int:
    x: int = 0
    def g() -> int:
        nonlocal x
        x = 1
        return x
    return g()

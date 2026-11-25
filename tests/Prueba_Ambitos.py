total: int = 1

class Base(object):
    value: int = 2

    def read(self: "Base") -> int:
        return self.value

class Derived(Base):
    def read(self: "Derived") -> int:
        return self.value + 1

def apply(n: int) -> int:
    item: Base = None

    def update() -> int:
        global total
        nonlocal n
        total = total + 1
        n = n + total
        return n

    def shadow(total: int) -> int:
        return total

    item = Derived()
    return shadow(update()) + item.read()

print(apply(3))

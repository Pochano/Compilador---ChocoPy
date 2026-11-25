class Animal(object):
    nombre: str = "Sin nombre"
    edad: int = 0

    def hablar(self: "Animal") -> str:
        return "Sonido generico"

def test():
    perro: Animal = None
    perro = Animal()
    print(perro.hablar())

test()

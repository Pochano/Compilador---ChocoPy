def factorial(n: int) -> int:
    result: int = 1
    i: int = 1
    while i <= n:
        result = result * i
        i = i + 1
    return result

def main():
    val: int = 5
    if val > 0:
        print("El factorial es:")
        print(factorial(val))
    else:
        print("El numero debe ser mayor a 0")

main()

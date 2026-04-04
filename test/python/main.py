import hello

if __name__ == "__main__":
    print("Hello from Python!")
    print(hello.say_hello())

    obj = hello.JlBTree()

    print("Result of insertion of new element: ", obj.insert(42))
    print("Result of redundant insertion: ", obj.insert(42))

    print("Result of deletion (tree does not contain element): ", obj.delete(41))
    print("Result of deletion (tree contains element): ", obj.delete(42))

    del obj

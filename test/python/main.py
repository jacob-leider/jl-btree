import jl_btree


def traverse(node, depth=0):
    print("  " * depth +
          f"Node with {node.num_keys()} keys and {node.num_children()} children")

    for i in range(node.num_keys()):
        print("  " * depth + f"Key {i}: {node.get_key(i)}")

    for i in range(node.num_children()):
        traverse(node.get_child(i), depth + 1)


if __name__ == "__main__":

    print("Hello from Python!")
    print(jl_btree.say_hello())

    obj = jl_btree.JlBTree()

    for i in range(100):
        k = jl_btree.JlBTreeKey(i)
        print(f"Inserting {i} into the B-tree: ",
              obj.insert(k))

    # Mess with keys
    root = obj.get_root()

    traverse(root)

    print("Root node number of keys: ", root.num_keys())
    print("Root node number of children: ", root.num_children())

    print("Result of deletion (tree does not contain element): ",
          obj.delete(jl_btree.JlBTreeKey(42)))
    print("Result of deletion (tree contains element): ",
          obj.delete(jl_btree.JlBTreeKey(41)))

    del obj

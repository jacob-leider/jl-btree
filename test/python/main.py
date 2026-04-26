import jl_btree


def traverse(node, depth=0):
    print("  " * depth +
          f"Node with {node.num_keys()} keys and {node.num_children()} children")

    for i in range(node.num_keys()):
        print("  " * depth + f"Key {i}: {node.get_key(i)}")

    for i in range(node.num_children()):
        traverse(node.get_child(i), depth + 1)


def test_btree_node_set_key():
    print("Testing BTreeNode set_key...")

    node = jl_btree.JlBTreeNode(False)
    k = jl_btree.JlBTreeKey(42)
    node.set_key(0, k)


def test_btree_node_set_child():
    print("Testing BTreeNode set_child...")

    a = jl_btree.JlBTreeNode(True)
    b = jl_btree.JlBTreeNode(False)
    k = jl_btree.JlBTreeKey(42)

    b.set_key(0, k)
    b.set_num_keys(1)

    a.set_child(0, b)
    a.set_num_children(1)

    k2 = a.get_child(0).get_key(0)

    print(
        f"Retrieved key from child: {k2}")
    print(f"As bytes: {k2.as_bytes()}")


def test_btree_insert_key():
    print("Testing BTree insert_key...")

    obj = jl_btree.JlBTree()
    k = jl_btree.JlBTreeKey(42)

    assert obj.insert(k) == 1
    assert obj.insert(k) == 2

    del obj


if __name__ == "__main__":

    print("Hello from Python!")
    print(jl_btree.say_hello())

    test_btree_node_set_key()
    test_btree_node_set_child()
    test_btree_insert_key()

    obj = jl_btree.JlBTree()

    del obj

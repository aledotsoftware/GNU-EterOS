with open('kernel/mm/heap.c', 'r') as f:
    content = f.read()

# Let's add a debug print in _kfree_impl to see what's being freed before panicking

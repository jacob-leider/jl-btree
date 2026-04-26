#!/usr/bin/env python3

# pip3 install --upgrade setuptools wheel

# See https://docs.python.org/3/extending/newtypes_tutorial.html

from setuptools import setup, Extension
import os


def get_btree_core_sources():
    core_dir = os.path.join(os.path.dirname(__file__), '../../src/core')
    return [os.path.join(core_dir, f) for f in os.listdir(core_dir) if f.endswith('.c')]


if __name__ == "__main__":
    # Set the SDK to an available version
    os.environ['SDKROOT'] = '/Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk'

    all_sources = get_btree_core_sources(
    ) + ["./bindings/jl_btree_bindings.c"]

    module = Extension("jl_btree", sources=all_sources)

    setup(
        name="jl_btree",
        version="1.0",
        description="A starter C extension for Python",
        ext_modules=[module],
        extra_compile_args=['-g', '-O0']
    )

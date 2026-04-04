# pip3 install --upgrade setuptools wheel

from setuptools import setup, Extension
import os

# Set the SDK to an available version
os.environ['SDKROOT'] = '/Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk'

module = Extension("hello", sources=["hello.c"])

setup(
    name="HelloModule",
    version="1.0",
    description="A starter C extension for Python",
    ext_modules=[module],
)

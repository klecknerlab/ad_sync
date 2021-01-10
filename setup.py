from setuptools import setup

with open("README.md", "r") as fh:
    long_description = fh.read()

setup(
    name='ad_sync',
    version='0.1',
    description='AD Synchronizer Python Driver.  Developed by the Kleckner/MUVI Labs at UC Merced.',
    url='https://github.com/klecknerlab/ad_sync',
    long_description=long_description,
    long_description_content_type="text/markdown",
    author='Dustin Kleckner',
    author_email='dkleckner@ucmerced.edu',
    license='Apache 2.0 (http://www.apache.org/licenses/LICENSE-2.0)',
    packages=['ad_sync'],
    install_requires=[],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Development Status :: 4 - Beta",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
    ],
)

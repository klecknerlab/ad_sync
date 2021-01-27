from setuptools import setup
import codecs

with codecs.open("README.md", "r", "utf-8") as f:
    long_description = f.read()

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
    install_requires=['pyserial'],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Development Status :: 4 - Beta",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
    ],
    entry_points={
        'gui_scripts': ['muvi_sync=ad_sync.gui:spawn']
    },
)

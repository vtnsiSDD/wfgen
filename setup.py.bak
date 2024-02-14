#!/usr/bin/env python

import os
from setuptools import setup,find_packages
from distutils.util import convert_path

ROOT = os.path.abspath(os.path.dirname(__file__))

def load_requirements(path_dir=ROOT, comment_char='#'):
    with open(os.path.join(path_dir,'requirements.txt'),'r') as fp:
        lines = [ln.strip() for ln in fp.readlines()]
    reqs = []
    for ln in lines:
        if comment_char in ln:
            ln = ln[:ln.index(comment_char)].strip()
        if ln:
            reqs.append(ln)
    return reqs

repo_info = {}
ver_path = convert_path('py_src/wfgen/_version.py')
if os.path.exists(ver_path):
    with open(ver_path) as ver_file:
        exec(ver_file.read(),repo_info)

setup(
    name        = 'wfgen',
    version     = repo_info['__version__'] if '__version__' in repo_info else '0.0+beta',
    description = repo_info['__docs__'] if '__docs__' in repo_info else 'beta',
    author      = repo_info['__author__'] if '__author__' in repo_info else 'beta',
    license     = repo_info['__license__'] if '__license__' in repo_info else 'beta',
    packages    = find_packages(exclude=['scripts','docs']),
    scripts     = [os.path.join(ROOT,'scripts',x) for x in ['consolidate_reports.py','proc_query.py','wav_server.py','wfgen_env.sh']],

    long_description                = open(os.path.join(ROOT,'README.md'), encoding='utf-8').read(),
    long_description_content_type   = 'text/markdown',
    include_package_data            = True,
    zip_safe                        = False,

    keywords = [],
    python_requires = '>=3.8',
    setup_requires = [],
    install_requires = load_requirements(ROOT),

    classifiers= []
)
            

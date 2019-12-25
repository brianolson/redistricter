#!/usr/bin/python

import glob
import json
import os

from jinja2 import Template, Environment, FileSystemLoader

def tojson(val):
    return json.dumps(val)

_jinja_env = None

def _env():
    global _jinja_env
    if _jinja_env is None:
        templatedir = os.path.abspath(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), 'html'))
        loader = FileSystemLoader(templatedir)
        _jinja_env = Environment(loader=loader)
        _jinja_env.filters['tojson'] = tojson
    return _jinja_env

def render(templateName, contextDict):
    "for ('foo.html', {'blah':'baz'}) return html string"
    return _env().get_template(templateName).render(contextDict)

#!/usr/bin/python

import os

import django
import django.template
import django.template.loader as tload

# django settings
TEMPLATE_LOADERS = ('django.template.loaders.filesystem.Loader',)
TEMPLATE_DIRS = (
  os.path.join(os.path.dirname(__file__), 'html'),
  os.path.dirname(__file__))
TEMPLATE_DEBUG = True
SECRET_KEY = 'aoeuaoeuaoeu'

os.environ['DJANGO_SETTINGS_MODULE'] = __name__
#tload.settings = sys.modules[__name__]

def render(templateName, contextDict):
	tt = tload.get_template(templateName)
	context = django.template.Context(contextDict)
	renderout = tt.render(context)
	## TODO: unicode instead of str, better wsgi containers fix this?
	ustr = unicode(renderout)
	return ustr.encode('utf-8')

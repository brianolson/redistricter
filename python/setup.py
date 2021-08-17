from setuptools import setup

setup(
    name='bdistricting',
    version='0.1.0',
    description='bdistricting.com setup and util',
    author='Brian Olson',
    author_email='bolson@bolson.org',
    url='https://github.com/brianolson/redistricter',
    packages=['bdistricting'],
    package_dir={'bdistricting': 'bdistricting'},
    #scripts = ['bdistricting/crawl2020.py'],
    entry_points={
        'console_scripts':
        [
            'crawl2020 = bdistricting.crawl2020:main',
            'run_redistricter = bdistricting.run_redistricter:main',
            'dataserver = bdistricting.dataserver:main',
            'analyze_submissions = bdistricting.analyze_submissions:main',
            'receiver_tool = bdistricting.receiver_tool:main',
        ]
    },
    license='BSD',
    classifiers=[
        'Programming Language :: Python :: 3.8',
    ]
)

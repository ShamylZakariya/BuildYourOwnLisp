project('clisp', 'c', version: '0.1', default_options : [
    'c_std=c11'])

# external dependencies
deps = [
    dependency('libedit', required: true)
]

# bring in local libs
subdir('libclisp')
subdir('libmpc')

# build the main executable
app_sources = [
    'main.c'
]

executable('clisp',
    sources: app_sources,
    link_with: [clisp_lib, mpc_lib],
    dependencies: deps)

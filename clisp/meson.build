project('clisp', 'c', version: '0.1', default_options : [
    'c_std=c11'])


# defines clisp_lib
subdir('libclisp')

# build the main executable
app_sources = [
    'main.c'
]

executable('clisp',
    sources: app_sources,
    link_with: clisp_lib)

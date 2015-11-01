#!/usr/bin/env nodejs
// Generates Ninja file for building.
var fs = require("fs");
var filename = "build.ninja";
var outdir = "out/";

var data = [
    'cxxflags = -O2 -g --std=c++14 -Wall -Werror -D_FILE_OFFSET_BITS=64',
    // TODO only link what is required for each binary.
    'ldflags = -lpthread -lfuse -lgit2 -lz ',
    'gxx = g++',
    'gcc = gcc',
    'rule cc',
    '  command = $gxx -MMD -MF $out.d $cxxflags -c $in -o $out',
    '  depfile = $out.d',
    '  deps = gcc',
    'rule cclink',
    '  command = $gxx $ldflags $in -o $out',
    'rule runtest',
    '  command = ./$in > $out.tmp 2>&1 && mv $out.tmp $out || ( cat $out.tmp; exit 1 )',
    'rule ninjagenerator',
    '  command = ./$in',
    '  generator = true',
    'build build.ninja: ninjagenerator configure.js'];

var compile_candidates = {};
function CompileLink(target, sources, opts) {
    function Link(target, sources) {
	data.push('build ' + target + ': cclink ' + sources.join(' '))
    }
    var sources_o = [];
    for (var i in sources) {
	compile_candidates[sources[i]] = true;
	sources_o.push(outdir + sources[i] + '.o');
    }
    if (!!opts && opts.extra_objects)
	sources_o = sources_o.concat(opts.extra_objects);
    Link(outdir + target, sources_o);
}
function Emit() {
    function EmitCompile() {
	for (i in compile_candidates) {
	    data.push('build ' + outdir + i +'.o: cc ' + i + '.cc')
	}
    }

    EmitCompile();

    fs.writeFileSync(filename, data.join('\n') +
		     // Last line needs to be terminated too.
		     '\n');
}

function RunTest(test, opts) {
    test = outdir + test;
    stdout = test + '.result';
    data.push('build ' + stdout + ': runtest ' + test)
}

function CompileLinkRunTest(target, sources, opts) {
    CompileLink(target, sources, opts);
    RunTest(target, opts);
}

// Rules
CompileLink('gitfs', ['gitfs', 'gitfs_fusemain', 'strutil',
		      'get_current_dir', 'gitxx', 'basename'])
CompileLinkRunTest('gitfs_test', ['gitfs', 'gitfs_test', 'strutil',
				  'get_current_dir', 'gitxx', 'basename'])
CompileLinkRunTest('gitlstree_test', ['gitlstree', 'gitlstree_test', 'strutil',
				      'get_current_dir', 'basename',
				      'concurrency_limit'])
CompileLink('gitlstree', ['gitlstree', 'gitlstree_fusemain', 'strutil',
			  'get_current_dir', 'basename', 'concurrency_limit'])
CompileLink('ninjafs', ['ninjafs', 'strutil', 'get_current_dir',
			'basename'])
CompileLink('hello_world', ['hello_world'])
CompileLinkRunTest('libgit2test', ['libgit2test', 'gitxx'])
CompileLinkRunTest('basename_test', ['basename_test', 'basename'])
CompileLink('hello_fuseflags', ['hello_fuseflags'])
CompileLinkRunTest('git-githubfs_test', ['git-githubfs_test', 'git-githubfs',
					 'strutil', 'concurrency_limit'], {
    extra_objects: ['/usr/lib/libjson_spirit.a']})
CompileLink('git-githubfs', ['git-githubfs_fusemain', 'git-githubfs',
			     'strutil', 'concurrency_limit'], {
    extra_objects: ['/usr/lib/libjson_spirit.a']})
CompileLinkRunTest('concurrency_limit_test', ['concurrency_limit_test',
					      'concurrency_limit']);
CompileLinkRunTest('directory_container_test', ['directory_container_test',
						'basename']);

CompileLink('git_ioctl_client', ['git_ioctl_client']);
CompileLinkRunTest('scoped_fd_test', ['scoped_fd_test']);
CompileLinkRunTest('cached_file_test', ['cached_file', 'cached_file_test']);

Emit()

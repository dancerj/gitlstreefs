#!/usr/bin/env nodejs
// Generates Ninja file for building.
var fs = require("fs");
var filename = "build.ninja";
var outdir = "out/";

var data = [
    'cxxflags = -O2 -g --std=c++14 -Wall -Werror -D_FILE_OFFSET_BITS=64 -I.',
    'ldflags = -pthread -lfuse',
    'gxx = g++',
    'gcc = gcc',
    'rule cc',
    '  command = $gxx -MMD -MF $out.d $cxxflags -c $in -o $out',
    '  depfile = $out.d',
    '  deps = gcc',
    'rule cclink',
    '  command = $gxx $in -o $out $ldflags',
    'rule cclinkwithgit2',
    '  command = $gxx $in -o $out -lgit2 $ldflags',
    'rule cclinkcowfs',
    '  command = $gxx $in -o $out -lboost_filesystem -lboost_system -lgcrypt $ldflags',
    'rule runtest',
    '  command = ./$in > $out.tmp 2>&1 && mv $out.tmp $out || ( cat $out.tmp; exit 1 )',
    'rule ninjagenerator',
    '  command = ./$in',
    '  generator = true',
    'build build.ninja: ninjagenerator configure.js'];

var compile_candidates = {};
function CompileLink(target, sources, opts) {
    function Link(target, sources) {
	var cclink = (!!opts && opts.cclink) || 'cclink'
	data.push('build ' + target + ': ' + cclink + ' ' + sources.join(' '))
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
    data.push('build ' + stdout + ': runtest ' + test +
	      ((!!opts && opts.extra_depends) ? '|' + opts.extra_depends : '' ))
}

function RunTestScript(test, opts) {
    test = test;
    stdout = outdir + test + '.result';
    data.push('build ' + stdout + ': runtest ' + test +
	      ((!!opts && opts.extra_depends) ? '|' + opts.extra_depends : '' ))
}

function CompileLinkRunTest(target, sources, opts) {
    CompileLink(target, sources, opts);
    RunTest(target, opts);
}

// Rules
CompileLinkRunTest('gitlstree_test',
		   ['basename',
		    'cached_file',
		    'concurrency_limit',
		    'get_current_dir',
		    'gitlstree',
		    'gitlstree_test',
		    'strutil'])

CompileLink('gitlstree',
	    ['basename',
	     'cached_file',
	     'concurrency_limit',
	     'get_current_dir',
	     'gitlstree',
	     'gitlstree_fusemain',
	     'strutil'])

CompileLinkRunTest('strutil_test', ['strutil', 'strutil_test'])
CompileLink('ninjafs', ['ninjafs', 'strutil', 'get_current_dir',
			'basename'])
RunTestScript('ninjafs_test.sh', {
    extra_depends: ['out/ninjafs']
})
CompileLink('hello_world', ['hello_world'])
CompileLinkRunTest('basename_test', ['basename_test', 'basename'])
CompileLinkRunTest('git-githubfs_test', ['base64decode', 'concurrency_limit',
					 'git-githubfs_test', 'git-githubfs',
					 'strutil'], {
    extra_objects: ['/usr/lib/libjson_spirit.a']})
CompileLink('git-githubfs', ['base64decode',
			     'concurrency_limit', 'git-githubfs_fusemain',
			     'git-githubfs',
			     'strutil'], {
    extra_objects: ['/usr/lib/libjson_spirit.a']})
CompileLinkRunTest('concurrency_limit_test', ['concurrency_limit_test',
					      'concurrency_limit']);
CompileLinkRunTest('directory_container_test', ['directory_container_test',
						'basename']);

CompileLink('git_ioctl_client', ['git_ioctl_client']);
CompileLinkRunTest('scoped_fd_test', ['scoped_fd_test']);
CompileLinkRunTest('cached_file_test', ['cached_file', 'cached_file_test']);
CompileLinkRunTest('base64decode_test', ['base64decode',
					 'base64decode_test']);

//Experimental code.
CompileLink('experimental/gitfs',
	    ['experimental/gitfs', 'experimental/gitfs_fusemain', 'strutil',
	     'get_current_dir', 'experimental/gitxx', 'basename'],
	    {cclink: 'cclinkwithgit2'})
CompileLinkRunTest('experimental/gitfs_test',
		   ['experimental/gitfs', 'experimental/gitfs_test', 'strutil',
		    'get_current_dir', 'experimental/gitxx', 'basename'],
		   {cclink: 'cclinkwithgit2'})
CompileLinkRunTest('experimental/libgit2test',
		   ['experimental/libgit2test', 'experimental/gitxx'],
		   {cclink: 'cclinkwithgit2'})
CompileLink('experimental/hello_fuseflags', ['experimental/hello_fuseflags'])
CompileLink('experimental/unkofs', ['experimental/unkofs', 'relative_path'])
CompileLink('experimental/globfs', ['experimental/globfs', 'relative_path'])
RunTestScript('experimental/globfs_test.sh', {
    extra_depends: ['out/experimental/globfs']
})
CompileLink('cowfs', ['cowfs', 'cowfs_crypt', 'file_copy',
		      'relative_path', 'scoped_fileutil', 'strutil'],
	    {cclink: 'cclinkcowfs'})
CompileLinkRunTest('cowfs_crypt_test', ['cowfs_crypt',
					'cowfs_crypt_test'],
		   {cclink: 'cclinkcowfs'});
RunTestScript('cowfs_test.sh', {
    extra_depends: ['out/cowfs']
})
CompileLink('file_copy_test', ['file_copy', 'file_copy_test'])
CompileLink('experimental/parallel_writer', ['experimental/parallel_writer'])
CompileLinkRunTest('scoped_fileutil_test',
		   ['scoped_fileutil_test', 'scoped_fileutil'])

Emit()

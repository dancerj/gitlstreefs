#!/usr/bin/env nodejs
// Test implementation of ninja-generator.
var fs = require("fs");
var filename = "build.ninja";
var outdir = "out/";

var data = [
    'cxxflags = -O2 -g --std=c++11 -Wall -Werror -D_FILE_OFFSET_BITS=64',
    'ldflags = -lpthread -lfuse -lgit2 -lz',
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
function CompileLink(target, sources) {
    function Link(target, sources) {
	data.push('build ' + target + ': cclink ' + sources.join(' '))
    }
    var sources_o = [];
    for (var i in sources) {
	compile_candidates[sources[i]] = true;
	sources_o.push(outdir + sources[i] + '.o');
    }
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

function RunTest(test, stdout) {
    test = outdir + test;
    if (stdout === undefined) {
	stdout = test + '.result';
    }
    data.push('build ' + stdout + ': runtest ' + test)
}

// Rules
CompileLink('gitfs', ['gitfs', 'gitfs_fusemain', 'strutil',
		      'get_current_dir', 'gitxx'])
CompileLink('gitfs_test', ['gitfs', 'gitfs_test', 'strutil',
			   'get_current_dir', 'gitxx'])
CompileLink('gitlstree_test', ['gitlstree', 'gitlstree_test', 'strutil',
			       'get_current_dir'])
CompileLink('gitlstree', ['gitlstree', 'gitlstree_fusemain', 'strutil',
			  'get_current_dir'])
CompileLink('ninjafs', ['ninjafs', 'strutil', 'get_current_dir'])
CompileLink('hello_world', ['hello_world'])
CompileLink('libgit2test', ['libgit2test', 'gitxx'])
RunTest('gitfs_test')
RunTest('gitlstree_test')
RunTest('libgit2test')
Emit()

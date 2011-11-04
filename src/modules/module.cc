// Copyright Joyent, Inc. and other Node contributors.
//           (c) 2011 the gearbox-node project authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <gearbox.h>

using namespace Gearbox;

/** \file src/modules/module.cc converted from src/modules/module.gear */

#line 1 "src/modules/module.gear"


#line 32 "src/modules/module.cc"
static void _setup_module(Value exports, Value require, Value module) {
    Context::getCurrent()->runScript("(function(exports, require, module){\n//BEGIN lib/module.js\nvar NativeModule = require('native_module');\n//BEGIN *gearbox\n//var Script = process.binding('evals').NodeScript;\nvar Script = require('vm');\n//END *gearbox\nvar runInThisContext = Script.runInThisContext;\nvar runInNewContext = Script.runInNewContext;\nvar assert = require('assert').ok;\n\n\n// If obj.hasOwnProperty has been overridden, then calling\n// obj.hasOwnProperty(prop) will break.\n// See: https://github.com/joyent/node/issues/1707\nfunction hasOwnProperty(obj, prop) {\n  return Object.prototype.hasOwnProperty.call(obj, prop);\n}\n\n\nfunction Module(id, parent) {\n  this.id = id;\n  this.exports = {};\n  this.parent = parent;\n\n  this.filename = null;\n  this.loaded = false;\n  this.exited = false;\n  this.children = [];\n}\nmodule.exports = Module;\n\n// Set the environ variable NODE_MODULE_CONTEXTS=1 to make node load all\n// modules in thier own context.\nModule._contextLoad = (+process.env['NODE_MODULE_CONTEXTS'] > 0);\nModule._cache = {};\nModule._pathCache = {};\nModule._extensions = {};\nvar modulePaths = [];\nModule.globalPaths = [];\n\nModule.wrapper = NativeModule.wrapper;\nModule.wrap = NativeModule.wrap;\n\nvar path = NativeModule.require('path');\n\nModule._debug = function() {};\nif (process.env.NODE_DEBUG && /module/.test(process.env.NODE_DEBUG)) {\n  Module._debug = function(x) {\n    console.error(x);\n  };\n}\n\n\n// We use this alias for the preprocessor that filters it out\nvar debug = Module._debug;\n\n\n// given a module name, and a list of paths to test, returns the first\n// matching file in the following precedence.\n//\n// require(\"a.<ext>\")\n//   -> a.<ext>\n//\n// require(\"a\")\n//   -> a\n//   -> a.<ext>\n//   -> a/index.<ext>\n\nfunction statPath(path) {\n  var fs = NativeModule.require('fs');\n  try {\n    return fs.statSync(path);\n  } catch (ex) {}\n  return false;\n}\n\n// check if the directory is a package.json dir\nvar packageCache = {};\n\nfunction readPackage(requestPath) {\n  if (hasOwnProperty(packageCache, requestPath)) {\n    return packageCache[requestPath];\n  }\n\n  var fs = NativeModule.require('fs');\n  try {\n    var jsonPath = path.resolve(requestPath, 'package.json');\n    var json = fs.readFileSync(jsonPath, 'utf8');\n  } catch (e) {\n    return false;\n  }\n\n  try {\n    var pkg = packageCache[requestPath] = JSON.parse(json);\n  } catch (e) {\n    e.path = jsonPath;\n    e.message = 'Error parsing ' + jsonPath + ': ' + e.message;\n    throw e;\n  }\n  return pkg;\n}\n\nfunction tryPackage(requestPath, exts) {\n  var pkg = readPackage(requestPath);\n\n  if (!pkg || !pkg.main) return false;\n\n  var filename = path.resolve(requestPath, pkg.main);\n  return tryFile(filename) || tryExtensions(filename, exts) ||\n         tryExtensions(path.resolve(filename, 'index'), exts);\n}\n\n// In order to minimize unnecessary lstat() calls,\n// this cache is a list of known-real paths.\n// Set to an empty object to reset.\nModule._realpathCache = {};\n\n// check if the file exists and is not a directory\nfunction tryFile(requestPath) {\n  var fs = NativeModule.require('fs');\n  var stats = statPath(requestPath);\n  if (stats && !stats.isDirectory()) {\n    return fs.realpathSync(requestPath, Module._realpathCache);\n  }\n  return false;\n}\n\n// given a path check a the file exists with any of the set extensions\nfunction tryExtensions(p, exts) {\n  for (var i = 0, EL = exts.length; i < EL; i++) {\n    var filename = tryFile(p + exts[i]);\n\n    if (filename) {\n      return filename;\n    }\n  }\n  return false;\n}\n\n\nModule._findPath = function(request, paths) {\n  var fs = NativeModule.require('fs');\n  var exts = Object.keys(Module._extensions);\n\n  if (request.charAt(0) === '/') {\n    paths = [''];\n  }\n\n  var trailingSlash = (request.slice(-1) === '/');\n\n  var cacheKey = JSON.stringify({request: request, paths: paths});\n  if (Module._pathCache[cacheKey]) {\n    return Module._pathCache[cacheKey];\n  }\n\n  // For each path\n  for (var i = 0, PL = paths.length; i < PL; i++) {\n    var basePath = path.resolve(paths[i], request);\n    var filename;\n\n    if (!trailingSlash) {\n      // try to join the request to the path\n      filename = tryFile(basePath);\n\n      if (!filename && !trailingSlash) {\n        // try it with each of the extensions\n        filename = tryExtensions(basePath, exts);\n      }\n    }\n\n    if (!filename) {\n      filename = tryPackage(basePath, exts);\n    }\n\n    if (!filename) {\n      // try it with each of the extensions at \"index\"\n      filename = tryExtensions(path.resolve(basePath, 'index'), exts);\n    }\n\n    if (filename) {\n      Module._pathCache[cacheKey] = filename;\n      return filename;\n    }\n  }\n  return false;\n};\n\n// 'from' is the __dirname of the module.\nModule._nodeModulePaths = function(from) {\n  // guarantee that 'from' is absolute.\n  from = path.resolve(from);\n\n  // note: this approach *only* works when the path is guaranteed\n  // to be absolute.  Doing a fully-edge-case-correct path.split\n  // that works on both Windows and Posix is non-trivial.\n  var splitRe = process.platform === 'win32' ? /[\\/\\\\]/ : /\\//;\n  // yes, '/' works on both, but let's be a little canonical.\n  var joiner = process.platform === 'win32' ? '\\\\' : '/';\n  var paths = [];\n  var parts = from.split(splitRe);\n\n  for (var tip = parts.length - 1; tip >= 0; tip--) {\n    // don't search in .../node_modules/node_modules\n    if (parts[tip] === 'node_modules') continue;\n    var dir = parts.slice(0, tip + 1).concat('node_modules').join(joiner);\n    paths.push(dir);\n  }\n\n  return paths;\n};\n\n\nModule._resolveLookupPaths = function(request, parent) {\n  if (NativeModule.exists(request)) {\n    return [request, []];\n  }\n\n  var start = request.substring(0, 2);\n  if (start !== './' && start !== '..') {\n    var paths = modulePaths;\n    if (parent) {\n      if (!parent.paths) parent.paths = [];\n      paths = parent.paths.concat(paths);\n    }\n    return [request, paths];\n  }\n\n  // with --eval, parent.id is not set and parent.filename is null\n  if (!parent || !parent.id || !parent.filename) {\n    // make require('./path/to/foo') work - normally the path is taken\n    // from realpath(__filename) but with eval there is no filename\n    var mainPaths = ['.'].concat(modulePaths);\n    mainPaths = Module._nodeModulePaths('.').concat(mainPaths);\n    return [request, mainPaths];\n  }\n\n  // Is the parent an index module?\n  // We can assume the parent has a valid extension,\n  // as it already has been accepted as a module.\n  var isIndex = /^index\\.\\w+?$/.test(path.basename(parent.filename));\n  var parentIdPath = isIndex ? parent.id : path.dirname(parent.id);\n  var id = path.resolve(parentIdPath, request);\n\n  // make sure require('./path') and require('path') get distinct ids, even\n  // when called from the toplevel js file\n  if (parentIdPath === '.' && id.indexOf('/') === -1) {\n    id = './' + id;\n  }\n\n  debug('RELATIVE: requested:' + request +\n        ' set ID to: ' + id + ' from ' + parent.id);\n\n  return [id, [path.dirname(parent.filename)]];\n};\n\n\nModule._load = function(request, parent, isMain) {\n  if (parent) {\n    debug('Module._load REQUEST  ' + (request) + ' parent: ' + parent.id);\n  }\n\n  var resolved = Module._resolveFilename(request, parent);\n  var id = resolved[0];\n  var filename = resolved[1];\n\n  var cachedModule = Module._cache[filename];\n  if (cachedModule) {\n    return cachedModule.exports;\n  }\n\n  if (NativeModule.exists(id)) {\n    // REPL is a special case, because it needs the real require.\n    if (id == 'repl') {\n//BEGIN *gearbox\n      var replModule = new Module('repl');\n      //replModule._compile(NativeModule.getSource('repl'), 'repl.js');\n      //NativeModule._cache.repl = replModule;\n      replModule._compile(null, 'repl.js');\n      return replModule.exports;\n//END *gearbox\n    }\n\n    debug('load native module ' + request);\n    return NativeModule.require(id);\n  }\n\n  var module = new Module(id, parent);\n\n  if (isMain) {\n    process.mainModule = module;\n    module.id = '.';\n  }\n\n  Module._cache[filename] = module;\n  try {\n    module.load(filename);\n  } catch (err) {\n    delete Module._cache[filename];\n    throw err;\n  }\n\n  return module.exports;\n};\n\nModule._resolveFilename = function(request, parent) {\n  if (NativeModule.exists(request)) {\n    return [request, request];\n  }\n\n  var resolvedModule = Module._resolveLookupPaths(request, parent);\n  var id = resolvedModule[0];\n  var paths = resolvedModule[1];\n\n  // look up the filename first, since that's the cache key.\n  debug('looking for ' + JSON.stringify(id) +\n        ' in ' + JSON.stringify(paths));\n\n  var filename = Module._findPath(request, paths);\n  if (!filename) {\n    throw new Error(\"Cannot find module '\" + request + \"'\");\n  }\n  id = filename;\n  return [id, filename];\n};\n\n\nModule.prototype.load = function(filename) {\n  debug('load ' + JSON.stringify(filename) +\n        ' for module ' + JSON.stringify(this.id));\n\n  assert(!this.loaded);\n  this.filename = filename;\n  this.paths = Module._nodeModulePaths(path.dirname(filename));\n\n  var extension = path.extname(filename) || '.js';\n  if (!Module._extensions[extension]) extension = '.js';\n  Module._extensions[extension](this, filename);\n  this.loaded = true;\n};\n\n\nModule.prototype.require = function(path) {\n  return Module._load(path, this);\n};\n\n\n// Returns exception if any\nModule.prototype._compile = function(content, filename) {\n  var self = this;\n  // remove shebang\n//BEGIN *gearbox\n  //content = content.replace(/^\\#\\!.*/, '');\n  content = content ? content.replace(/^\\#\\!.*/, '') : content;\n//END *gearbox\n\n  function require(path) {\n    return self.require(path);\n  }\n\n  require.resolve = function(request) {\n    return Module._resolveFilename(request, self)[1];\n  };\n\n  Object.defineProperty(require, 'paths', { get: function() {\n    throw new Error('require.paths is removed. Use ' +\n                    'node_modules folders, or the NODE_PATH ' +\n                    'environment variable instead.');\n  }});\n\n  require.main = process.mainModule;\n\n  // Enable support to add extra extension types\n  require.extensions = Module._extensions;\n  require.registerExtension = function() {\n    throw new Error('require.registerExtension() removed. Use ' +\n                    'require.extensions instead.');\n  };\n\n  require.cache = Module._cache;\n  \n//BEGIN *gearbox\n  if(content === null && filename == 'repl.js')\n      return self.exports = NativeModule.require('repl', require);\n//END *gearbox\n\n  var dirname = path.dirname(filename);\n\n  if (Module._contextLoad) {\n    if (self.id !== '.') {\n      debug('load submodule');\n      // not root module\n      var sandbox = {};\n      for (var k in global) {\n        sandbox[k] = global[k];\n      }\n      sandbox.require = require;\n      sandbox.exports = self.exports;\n      sandbox.__filename = filename;\n      sandbox.__dirname = dirname;\n      sandbox.module = self;\n      sandbox.global = sandbox;\n      sandbox.root = root;\n\n      return runInNewContext(content, sandbox, filename, true);\n    }\n\n    debug('load root module');\n    // root module\n    global.require = require;\n    global.exports = self.exports;\n    global.__filename = filename;\n    global.__dirname = dirname;\n    global.module = self;\n\n    return runInThisContext(content, filename, true);\n  }\n\n  // create wrapper function\n  var wrapper = Module.wrap(content);\n\n  var compiledWrapper = runInThisContext(wrapper, filename, true);\n  if (filename === process.argv[1] && global.v8debug) {\n    global.v8debug.Debug.setBreakPoint(compiledWrapper, 0, 0);\n  }\n  var args = [self.exports, require, self, filename, dirname];\n  return compiledWrapper.apply(self.exports, args);\n};\n\n\nfunction stripBOM(content) {\n  // Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)\n  // because the buffer-to-string conversion in `fs.readFileSync()`\n  // translates it to FEFF, the UTF-16 BOM.\n  if (content.charCodeAt(0) === 0xFEFF) {\n    content = content.slice(1);\n  }\n  return content;\n}\n\n\n// Native extension for .js\nModule._extensions['.js'] = function(module, filename) {\n  var content = NativeModule.require('fs').readFileSync(filename, 'utf8');\n  module._compile(stripBOM(content), filename);\n};\n\n\n// Native extension for .json\nModule._extensions['.json'] = function(module, filename) {\n  var content = NativeModule.require('fs').readFileSync(filename, 'utf8');\n  module.exports = JSON.parse(stripBOM(content));\n};\n\n\n//Native extension for .node\nModule._extensions['.node'] = function(module, filename) {\n  process.dlopen(filename, module.exports);\n};\n\n\n// bootstrap main module.\nModule.runMain = function() {\n  // Load the main module--the command line argument.\n  Module._load(process.argv[1], null, true);\n};\n\nModule._initPaths = function() {\n  var paths = [path.resolve(process.execPath, '..', '..', 'lib', 'node')];\n\n  if (process.env['HOME']) {\n    paths.unshift(path.resolve(process.env['HOME'], '.node_libraries'));\n    paths.unshift(path.resolve(process.env['HOME'], '.node_modules'));\n  }\n\n  if (process.env['NODE_PATH']) {\n    var splitter = process.platform === 'win32' ? ';' : ':';\n    paths = process.env['NODE_PATH'].split(splitter).concat(paths);\n  }\n\n  modulePaths = paths;\n\n  // clone as a read-only copy, for introspection.\n  Module.globalPaths = modulePaths.slice(0);\n};\n\n// bootstrap repl\nModule.requireRepl = function() {\n//BEGIN *gearbox\n  //return Module._load('repl', '.');\n  return Module._load('repl', null);\n//END *gearbox\n};\n\nModule._initPaths();\n\n// backwards compatibility\nModule.Module = Module;\n//END lib/module.js\n    })", "gear:module")(exports, require, module);
}
static NativeModule _module_module("module", _setup_module);
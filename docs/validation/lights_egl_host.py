"""Compile the existing Lights GLSL and read neutral-path pixels on host GLES 3.

Usage: python3 lights_egl_host.py ShaderLibrary.cpp path/to/Effects
Requires host EGL/Mesa. This is not an Android performance test.
"""
import ctypes as c
from pathlib import Path
import re
import sys

egl = c.CDLL("libEGL.so.1")
def egl_fn(name, result, *args):
    function = getattr(egl, name)
    function.restype, function.argtypes = result, list(args)
    return function
ptr, integer, uint = c.c_void_p, c.c_int, c.c_uint
get_proc = egl_fn("eglGetProcAddress", ptr, c.c_char_p)
platform = c.CFUNCTYPE(ptr, uint, ptr, c.POINTER(integer))(get_proc(b"eglGetPlatformDisplayEXT"))
display = platform(0x31DD, None, None)
major, minor = integer(), integer()
assert egl_fn("eglInitialize", uint, ptr, c.POINTER(integer), c.POINTER(integer))(display, c.byref(major), c.byref(minor))
assert egl_fn("eglBindAPI", uint, uint)(0x30A0)
attributes = (integer * 13)(0x3033, 1, 0x3040, 0x40, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038)
config, count = ptr(), integer()
assert egl_fn("eglChooseConfig", uint, ptr, c.POINTER(integer), c.POINTER(ptr), integer, c.POINTER(integer))(display, attributes, c.byref(config), 1, c.byref(count)) and count.value
surface = egl_fn("eglCreatePbufferSurface", ptr, ptr, ptr, c.POINTER(integer))(display, config, (integer * 5)(0x3057, 1, 0x3056, 1, 0x3038))
context = egl_fn("eglCreateContext", ptr, ptr, ptr, ptr, c.POINTER(integer))(display, config, None, (integer * 3)(0x3098, 3, 0x3038))
assert surface and context
assert egl_fn("eglMakeCurrent", uint, ptr, ptr, ptr, ptr)(display, surface, surface, context)
def gl(name, result, *args):
    address = get_proc(name.encode())
    assert address, name
    return c.CFUNCTYPE(result, *args)(address)
get_string = gl("glGetString", c.c_char_p, uint)
print("HOST_GLES_VERSION=" + get_string(0x1F02).decode())
print("HOST_GLES_RENDERER=" + get_string(0x1F01).decode())
create_shader = gl("glCreateShader", uint, uint)
shader_source = gl("glShaderSource", None, uint, integer, c.POINTER(c.c_char_p), c.POINTER(integer))
compile_shader = gl("glCompileShader", None, uint)
shader_status = gl("glGetShaderiv", None, uint, uint, c.POINTER(integer))
shader_log = gl("glGetShaderInfoLog", None, uint, integer, c.POINTER(integer), ptr)
def compile_source(kind, text):
    shader = create_shader(kind)
    source = c.c_char_p(text.encode())
    shader_source(shader, 1, c.byref(source), None)
    compile_shader(shader)
    status = integer()
    shader_status(shader, 0x8B81, c.byref(status))
    log = c.create_string_buffer(8192)
    shader_log(shader, len(log), None, log)
    assert status.value, log.value.decode()
    return shader
library = Path(sys.argv[1]).read_text()
vertex = re.search(r'kVertex = R"\((.*?)\)";', library, re.S)[1]
prelude = re.search(r'kPrelude = R"\((.*?)\)";', library, re.S)[1]
vs = compile_source(0x8B31, vertex)
vao, vbo, texture = uint(), uint(), uint()
gl("glGenVertexArrays", None, integer, c.POINTER(uint))(1, c.byref(vao))
gl("glBindVertexArray", None, uint)(vao)
gl("glGenBuffers", None, integer, c.POINTER(uint))(1, c.byref(vbo))
gl("glBindBuffer", None, uint, uint)(0x8892, vbo)
quad = (c.c_float * 8)(-1, -1, 1, -1, -1, 1, 1, 1)
gl("glBufferData", None, uint, c.c_ssize_t, ptr, uint)(0x8892, c.sizeof(quad), quad, 0x88E4)
gl("glVertexAttribPointer", None, uint, integer, uint, c.c_ubyte, integer, ptr)(0, 2, 0x1406, 0, 0, None)
gl("glEnableVertexAttribArray", None, uint)(0)
gl("glGenTextures", None, integer, c.POINTER(uint))(1, c.byref(texture))
gl("glBindTexture", None, uint, uint)(0x0DE1, texture)
for pname in (0x2800, 0x2801):
    gl("glTexParameteri", None, uint, uint, integer)(0x0DE1, pname, 0x2600)
source_pixel = (c.c_ubyte * 4)(32, 64, 96, 255)
gl("glTexImage2D", None, uint, integer, integer, integer, integer, integer, uint, uint, ptr)(0x0DE1, 0, 0x8058, 1, 1, 0, 0x1908, 0x1401, source_pixel)
gl("glViewport", None, integer, integer, integer, integer)(0, 0, 1, 1)
expected = {"ShadowMask.frag": (255, 255, 255, 0), "ShadowRays.frag": (0, 0, 0, 255), "LightCompose.frag": (32, 64, 96, 255)}
compiled, checked = 0, 0
for path in sorted(Path(sys.argv[2]).glob("*.frag")):
    body = path.read_text()
    assert "#include" not in body, "Use ShaderLibrary::BuildFragment for include-bearing shaders"
    fs = compile_source(0x8B30, "#version 300 es\n#line 1 1\n" + prelude + "#line 1 0\n" + body)
    program = gl("glCreateProgram", uint)()
    attach = gl("glAttachShader", None, uint, uint)
    attach(program, vs); attach(program, fs)
    gl("glLinkProgram", None, uint)(program)
    status = integer()
    gl("glGetProgramiv", None, uint, uint, c.POINTER(integer))(program, 0x8B82, c.byref(status))
    log = c.create_string_buffer(8192)
    gl("glGetProgramInfoLog", None, uint, integer, c.POINTER(integer), ptr)(program, len(log), None, log)
    assert status.value, (path.name, log.value.decode())
    compiled += 1
    if path.name in expected:
        gl("glUseProgram", None, uint)(program)
        location = gl("glGetUniformLocation", integer, uint, c.c_char_p)(program, b"uBox")
        gl("glUniform4f", None, integer, c.c_float, c.c_float, c.c_float, c.c_float)(location, 0, 0, 1, 1)
        gl("glDrawArrays", None, uint, integer, integer)(5, 0, 4)
        pixel = (c.c_ubyte * 4)()
        gl("glReadPixels", None, integer, integer, integer, integer, uint, uint, ptr)(0, 0, 1, 1, 0x1908, 0x1401, pixel)
        assert tuple(pixel) == expected[path.name], (path.name, tuple(pixel))
        checked += 1
    assert gl("glGetError", uint)() == 0, path.name
    gl("glDeleteProgram", None, uint)(program)
    gl("glDeleteShader", None, uint)(fs)
assert compiled == 6 and checked == 3
print("LIGHTS_HOST_GLES3_COMPILE_LINK_AND_NEUTRAL_PIXELS=PASS shaders=6 pixelCases=3")
egl_fn("eglMakeCurrent", uint, ptr, ptr, ptr, ptr)(display, None, None, None)
egl_fn("eglDestroyContext", uint, ptr, ptr)(display, context)
egl_fn("eglDestroySurface", uint, ptr, ptr)(display, surface)
egl_fn("eglTerminate", uint, ptr)(display)

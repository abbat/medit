# comp_name foo/foo-enums-in.py foo/foo-enums stamp

import sys
import filecmp
import shutil
import os
import re

def copy_tmp_if_changed(tmp, dst):
    docopy = False
    try:
        docopy = not filecmp.cmp(tmp, dst)
    except:
        docopy = True

    if docopy:
        shutil.copyfile(tmp, dst)

    try:
        os.remove(tmp)
    except:
        pass

comp_name = sys.argv[1]
input_py = sys.argv[2]
output_h = sys.argv[3] + '.h'
output_c = sys.argv[3] + '.c'
output_h_tmp = output_h + '.tmp'
output_c_tmp = output_c + '.tmp'
stamp = sys.argv[4]

vrs = {}
for a in sys.argv[5:]:
    key, val = a.split('=')
    vrs[key] = eval(val)
exec(compile(open(input_py, "rb").read(), input_py, 'exec'), vrs)

def parse_name(Name):
    Pieces = re.findall('[A-Z][a-z]+', Name)
    assert Name == ''.join(Pieces)
    PIECES = [s.upper() for s in Pieces]
    pieces = [s.lower() for s in Pieces]
    return { 'Name': Name, 'name': '_'.join(pieces),
             'Prefix': pieces[0], 'prefix': pieces[0].lower(), 'PREFIX': pieces[0].upper(),
             'short': '_'.join(pieces[1:]), 'SHORT': '_'.join(PIECES[1:]), }

def print_enum_h(name, vals, out):
    print('typedef enum {', file=out)
    for i in range(len(vals)):
        v = vals[i]
        out.write('    %s' % (v[0],))
        if v[1:] and v[1] is not None:
            out.write(' = %s' % (v[1],))
        if i + 1 < len(vals):
            out.write(',')
        if v[2:] and v[2] is not None:
            out.write(' /* %s */' % (v[2],))
        out.write('\n')
    print('} %s;' % name, file=out)
    print('', file=out)
    dic = parse_name(name)
    print('GType %(name)s_get_type (void) G_GNUC_CONST;' % dic, file=out)
    print('#define %(PREFIX)s_TYPE_%(SHORT)s (%(name)s_get_type())' % dic, file=out)

def print_flags_h(name, vals, out):
    print_enum_h(name, vals, out)

## h

out = open(output_h_tmp, 'w')

print('#ifndef %s_ENUMS_H' % (comp_name.upper(),), file=out)
print('#define %s_ENUMS_H' % (comp_name.upper(),), file=out)
print('', file=out)
print('#include <glib-object.h>', file=out)
print('', file=out)
print('G_BEGIN_DECLS', file=out)
print('', file=out)

for name in vrs.get('enums', {}):
    print_enum_h(name, vrs['enums'][name], out)
    print('', file=out)
for name in vrs.get('flags', {}):
    print_flags_h(name, vrs['flags'][name], out)
    print('', file=out)

print('', file=out)
print('G_END_DECLS', file=out)
print('', file=out)
print('#endif /* %s_ENUMS_H */' % (comp_name.upper(),), file=out)

out.close()
copy_tmp_if_changed(output_h_tmp, output_h)


## c

def print_flags_or_enum_c(name, vals, out, enum):
    dic = parse_name(name)
    dic['Type'] = enum and 'Enum' or 'Flags'
    dic['type'] = enum and 'enum' or 'flags'
    out.write('''\
/**
 * %(type)s:%(Name)s
 **/
GType\n%(name)s_get_type (void)
{
    static GType etype;
    if (G_UNLIKELY (!etype))
    {
        static const G%(Type)sValue values[] = {
''' % dic)

    for v in vals:
        name = v[0]
        nick = v[3] if v[3:] and v[3] else name
        out.write('            { %s, (char*) "%s", (char*) "%s" },\n' % (name, name, nick))

    out.write('''            { 0, NULL, NULL }
        };
        etype = g_%(type)s_register_static ("%(Name)s", values);
    }
    return etype;
}\n
''' % dic)

def print_enum_c(name, vals, out):
    print_flags_or_enum_c(name, vals, out, True)

def print_flags_c(name, vals, out):
    print_flags_or_enum_c(name, vals, out, False)

out = open(output_c_tmp, 'w')

print('#include "%s"' % (os.path.basename(output_h),), file=out)
print('', file=out)

for name in vrs.get('enums', {}):
    print_enum_c(name, vrs['enums'][name], out)
for name in vrs.get('flags', {}):
    print_flags_c(name, vrs['flags'][name], out)

print('', file=out)

out.close()
copy_tmp_if_changed(output_c_tmp, output_c)


## stamp

open(stamp, 'w').write('stamp\n')

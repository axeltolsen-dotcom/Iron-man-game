"""
edith_matlib.py - shared helpers for Edith's procedural PBR master materials
(create_suit_materials.py, create_enemy_materials.py). UE 5.8 editor Python.

UE 5.8 quirks handled here:
  * MaterialInstanceConstantFactoryNew has no initial_parent -> parent is set afterwards with
    MaterialEditingLibrary.set_material_instance_parent.
  * MI scalar/vector setters can return False even when they worked -> every value is read back.
  * delete_all_material_expressions only removes ~half per call -> loop until the graph is empty.
Everything is procedural: no external textures.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


class Log:
    def __init__(self, tag):
        self.tag, self.lines, self.errors = tag, [], []

    def __call__(self, msg):
        line = "%s %s" % (self.tag, msg)
        self.lines.append(line)
        print(line)
        unreal.log(line)

    def err(self, msg):
        self.errors.append(msg)
        self("ERROR " + msg)


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def load_or_create_material(folder, name, log):
    path = folder + "/" + name
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if mat is not None and isinstance(mat, unreal.Material):
        # Idempotent rebuild: empty the existing graph (keeps the asset + references stable).
        for _ in range(64):
            if mel.get_num_material_expressions(mat) == 0:
                break
            mel.delete_all_material_expressions(mat)
        left = mel.get_num_material_expressions(mat)
        log("reuse %s (graph cleared, %d expressions left)" % (path, left))
        if left:
            log.err("%s: could not clear graph (%d left)" % (path, left))
        return mat
    if mat is not None:
        eal.delete_asset(path)
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    log("created %s" % path)
    return mat


class G:
    """Tiny graph helper bound to one material."""

    def __init__(self, mat, log):
        self.mat, self.log = mat, log
        self.scalars, self.vectors = {}, {}

    def expr(self, cls, x, y, **props):
        e = mel.create_material_expression(self.mat, cls, x, y)
        for k, v in props.items():
            try:
                e.set_editor_property(k, v)
            except Exception as ex:  # optional props differ between engine versions
                self.log("  ~ %s.%s not set (%s)" % (cls.__name__, k, ex))
        return e

    def scalar(self, name, value, x, y, group="Edith"):
        e = self.expr(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=float(value), group=group)
        self.scalars[name] = e
        return e

    def vector(self, name, rgb, x, y, group="Edith"):
        e = self.expr(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                      default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0), group=group)
        self.vectors[name] = e
        return e

    def custom(self, code, inputs, x, y, out="float3", desc="Custom"):
        types = {"float1": unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                 "float2": unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                 "float3": unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                 "float4": unreal.CustomMaterialOutputType.CMOT_FLOAT4}
        c = mel.create_material_expression(self.mat, unreal.MaterialExpressionCustom, x, y)
        ins = []
        for n in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", n)
            ins.append(ci)
        c.set_editor_property("inputs", ins)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", types[out])
        c.set_editor_property("description", desc)
        return c

    def link(self, a, b, pin, out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            self.log.err("%s: link failed -> %s.%s" % (self.mat.get_name(), b.get_name(), pin))

    def prop(self, e, p, out=""):
        if not mel.connect_material_property(e, out, p):
            self.log.err("%s: property link failed -> %s" % (self.mat.get_name(), p))

    def mask(self, src, x, y, r=False, g=False, b=False, a=False):
        m = self.expr(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)
        self.link(src, m, "")
        return m

    # ---- inputs shared by both masters
    def local_position(self, x, y):
        if hasattr(unreal, "MaterialExpressionLocalPosition"):
            return self.expr(unreal.MaterialExpressionLocalPosition, x, y)
        wp = self.expr(unreal.MaterialExpressionWorldPosition, x - 200, y)
        op = self.expr(unreal.MaterialExpressionObjectPositionWS, x - 200, y + 80)
        sub = self.expr(unreal.MaterialExpressionSubtract, x, y)
        self.link(wp, sub, "A")
        self.link(op, sub, "B")
        return sub

    def camera_relative_wp(self, x, y):
        e = self.expr(unreal.MaterialExpressionWorldPosition, x, y)
        enum = getattr(unreal, "WorldPositionIncludedOffsets", None)
        val = getattr(enum, "WPT_CAMERA_RELATIVE", None) if enum else None
        if val is not None:
            e.set_editor_property("world_position_shader_offset", val)
        else:
            self.log("  ~ camera-relative world position not available; using absolute (LWC demoted)")
        return e

    def local_axis_ws(self, axis, x, y):
        """World-space image of a local unit axis (rotation * scale): TransformVector Local->World."""
        c = self.expr(unreal.MaterialExpressionConstant3Vector, x - 180, y,
                      constant=unreal.LinearColor(*(list(axis) + [1.0])))
        t = mel.create_material_expression(self.mat, unreal.MaterialExpressionTransform, x, y)
        try:
            t.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
            t.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        except Exception as ex:
            self.log.err("%s: transform setup failed (%s)" % (self.mat.get_name(), ex))
        self.link(c, t, "")
        return t

    def finish(self, path, expected_scalars, expected_vectors):
        mel.layout_material_expressions(self.mat)
        mel.recompile_material(self.mat)
        eal.save_asset(path, only_if_is_dirty=False)
        n = mel.get_num_material_expressions(self.mat)
        got_s = set(str(s) for s in mel.get_scalar_parameter_names(self.mat))
        got_v = set(str(s) for s in mel.get_vector_parameter_names(self.mat))
        miss = sorted(set(expected_scalars) - got_s) + sorted(set(expected_vectors) - got_v)
        self.log("%s: %d expressions, %d scalar + %d vector params" % (path, n, len(got_s), len(got_v)))
        if miss:
            self.log.err("%s: missing params %s" % (path, miss))
        try:
            st = mel.get_statistics(self.mat)
            self.log("%s: statistics ps=%s vs=%s samplers=%s (0 under -nullrhi = shaders not compiled)" % (
                path, st.get_editor_property("num_pixel_shader_instructions"),
                st.get_editor_property("num_vertex_shader_instructions"), st.get_editor_property("num_samplers")))
        except Exception as ex:
            self.log("%s: statistics unavailable (%s)" % (path, ex))
        return not miss


def make_mi(folder, name, parent, scalars, vectors, log):
    """Create/reuse an MI, set parent + params, verify by reading back. Returns True if all values stuck."""
    path = folder + "/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if mi is not None and not isinstance(mi, unreal.MaterialInstanceConstant):
        eal.delete_asset(path)
        mi = None
    if mi is None:
        mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if mi is None:
        log.err("%s: create_asset returned None" % path)
        return False
    mel.set_material_instance_parent(mi, parent)
    if mi.get_editor_property("parent") != parent:
        log.err("%s: parent did not stick" % path)
        return False
    if hasattr(mel, "clear_all_material_instance_parameters"):
        mel.clear_all_material_instance_parameters(mi)   # idempotent: drop stale overrides
    bad = []
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, float(v))   # return value unreliable in 5.8
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    mel.update_material_instance(mi)
    for k, v in scalars.items():
        got = mel.get_material_instance_scalar_parameter_value(mi, k)
        if abs(got - float(v)) > 1e-3:
            bad.append("%s=%s(want %s)" % (k, got, v))
    for k, v in vectors.items():
        got = mel.get_material_instance_vector_parameter_value(mi, k)
        if max(abs(got.r - v[0]), abs(got.g - v[1]), abs(got.b - v[2])) > 1e-3:
            bad.append("%s=(%.3f,%.3f,%.3f)(want %s)" % (k, got.r, got.g, got.b, tuple(v)))
    eal.save_asset(path, only_if_is_dirty=False)
    if bad:
        log.err("%s: params did not stick: %s" % (path, ", ".join(bad)))
        return False
    log("ok %s (%d scalars, %d vectors verified)" % (path, len(scalars), len(vectors)))
    return True

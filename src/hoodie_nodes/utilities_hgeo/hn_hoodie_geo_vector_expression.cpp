#include "hn_hoodie_geo_vector_expression.h"

#include <functional>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/expression.hpp>

using namespace godot;

void HNHoodieGeoVectorExpression::set_filter_expression(const String p_value) {
    filter_expression = p_value;
}

String HNHoodieGeoVectorExpression::get_filter_expression() const {
    return filter_expression;
}

void HNHoodieGeoVectorExpression::set_expression(const String p_value) {
    expression = p_value;
}

String HNHoodieGeoVectorExpression::get_expression() const {
    return expression;
}

void HNHoodieGeoVectorExpression::set_target(const Target p_value) {
    target = p_value;
}

HNHoodieGeoVectorExpression::Target HNHoodieGeoVectorExpression::get_target() const {
    return target;
}

void HNHoodieGeoVectorExpression::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_filter_expression", "value"), &HNHoodieGeoVectorExpression::set_filter_expression);
    ClassDB::bind_method(D_METHOD("get_filter_expression"), &HNHoodieGeoVectorExpression::get_filter_expression);
    ClassDB::bind_method(D_METHOD("set_expression", "value"), &HNHoodieGeoVectorExpression::set_expression);
    ClassDB::bind_method(D_METHOD("get_expression"), &HNHoodieGeoVectorExpression::get_expression);
    ClassDB::bind_method(D_METHOD("set_target", "value"), &HNHoodieGeoVectorExpression::set_target);
    ClassDB::bind_method(D_METHOD("get_target"), &HNHoodieGeoVectorExpression::get_target);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "Filter", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_filter_expression", "get_filter_expression");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "Expression", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_expression", "get_expression");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "Operation", PROPERTY_HINT_ENUM, "Points,PackedPoints", PROPERTY_USAGE_NO_EDITOR), "set_target", "get_target");

    BIND_ENUM_CONSTANT(Target::Points);
    BIND_ENUM_CONSTANT(Target::PackedPoints);
}

void HNHoodieGeoVectorExpression::_process(const Array &p_inputs) {
    Ref<HoodieGeo> in_hgeo = p_inputs[0];
    String attribute_name = "";

    {
        Array a = p_inputs[1];
        if (a.size() > 0) {
            attribute_name = a[0];
        }
    }

    bool use_attribute = false;

    UtilityFunctions::print("Attribute name: [" + attribute_name + "]");

    if (in_hgeo.is_null()) { return; }
    if (!attribute_name.is_empty()) {
        use_attribute = true;
        if (!in_hgeo->attributes.has(attribute_name)) { return; }
    }

    Ref<Expression> filter;
    filter.instantiate();

    PackedStringArray filter_params;
    filter_params.push_back("i");
    filter_params.push_back("ptscount");

    Error f_error = filter->parse(filter_expression, filter_params);
    if (f_error != Error::OK) {
        UtilityFunctions::print("Filter parse NOT OK.");
        return;
    }

    Vector<PackedVector3Array> pts_packs;
    Vector<HashMap<String, Array>> attr_packs;

    Vector<PackedVector3Array> out_pts_packs;
    Vector<HashMap<String, Array>> out_attr_packs;

    if (target == Target::Points) {
        pts_packs.push_back(in_hgeo->points.duplicate());
        attr_packs.push_back(in_hgeo->duplicate_attributes());
    } else if (target == Target::PackedPoints) {
        pts_packs = in_hgeo->pack_primitive_points();
        attr_packs = in_hgeo->pack_primitive_attributes();
    }

    for (int prim = 0; prim < pts_packs.size(); prim++) {
        PackedVector3Array pts = pts_packs[prim];
        HashMap<String, Array> attributes = attr_packs[prim];

        Array attr;
        if (use_attribute) {
            attr = attributes[attribute_name];

            if (attr.size() < 1) {
                continue;
            }
            // UtilityFunctions::print("Attribute type: " + Variant::get_type_name(attr[0].get_type()));
            if (!Variant::can_convert(attr[0].get_type(), Variant::VECTOR3) && !Variant::can_convert(attr[0].get_type(), Variant::FLOAT)) {
                return;
            }
        }

        Array target_arr = use_attribute ? Array(attr) : Array(pts);


        for (int i = 0; i < target_arr.size(); i++) {
            Array filter_values;
            filter_values.push_back(i);
            filter_values.push_back(target_arr.size());

            Variant filter_ret = filter->execute(filter_values);

            if (filter->has_execute_failed()) {
                UtilityFunctions::print("Filter execution failed.");
                return;
            }

            Ref<Expression> expr;
            expr.instantiate();

            PackedStringArray expr_params;
            expr_params.push_back("x");
            expr_params.push_back("y");
            expr_params.push_back("z");
            // if (use_attribute) { expr_params.push_back(attribute_name); }
            for (auto a : attributes) {
                expr_params.push_back(a.key);
            }

            Error expr_err = expr->parse(expression, expr_params);
            if (expr_err != Error::OK) {
                UtilityFunctions::print("Expr parse NOT OK.");
                return;
            }

            if ((bool)filter_ret) {
                Array expr_values;
                expr_values.push_back(pts[i].x);
                expr_values.push_back(pts[i].y);
                expr_values.push_back(pts[i].z);
                // if (use_attribute) { expr_values.push_back(Vector3(attr[i])); }
                for (auto a : attributes) {
                    expr_values.push_back(a.value[i]);
                }

                Variant expr_ret = expr->execute(expr_values);

                if (expr->has_execute_failed()) {
                    UtilityFunctions::print("Expression execution failed.");
                    return;
                }

                // Return value validation
                if (!Variant::can_convert(expr_ret.get_type(), Variant::Type::ARRAY)) {
                    return;
                }
                Array arr_ret = expr_ret;
                if (arr_ret.size() == 3) {
                    Variant x = arr_ret[0];
                    Variant y = arr_ret[1];
                    Variant z = arr_ret[2];
                    if (!Variant::can_convert(x.get_type(), Variant::FLOAT)) {
                        return;
                        }
                    if (!Variant::can_convert(y.get_type(), Variant::FLOAT)) {
                        return;
                    }
                    if (!Variant::can_convert(z.get_type(), Variant::FLOAT)) {
                        return;
                    }
                    target_arr[i] = Vector3(x, y, z);
                } else if (arr_ret.size() == 1) {
                    Variant x = arr_ret[0];
                    if (!Variant::can_convert(x.get_type(), Variant::FLOAT)) {
                        return;
                    }
                    target_arr[i] = x;
                }
            } else {
                target_arr[i] = use_attribute ? attr[i].operator godot::Vector3() : pts[i];

            }
        }

        if (use_attribute) {
            attributes[attribute_name] = target_arr;
            out_attr_packs.push_back(attributes);
            out_pts_packs.push_back(pts);
        } else {
            out_pts_packs.push_back(target_arr);
        }
    }

    Ref<HoodieGeo> out_hgeo = in_hgeo->duplicate();

    if (target == Target::Points) {
        if (use_attribute) {
            out_hgeo->attributes[attribute_name] = out_attr_packs[0][attribute_name];
        } else {
            out_hgeo->points = out_pts_packs[0];
        }
    } else if (target == Target::PackedPoints) {
        out_hgeo->unpack_primitive_points(out_pts_packs);
        
        if (use_attribute) {
            out_hgeo->unpack_primitive_attributes(out_attr_packs);
        }
    }

    set_output(0, out_hgeo);
}

String HNHoodieGeoVectorExpression::get_caption() const {
    return "HGeo Vector Expr";
}

int HNHoodieGeoVectorExpression::get_input_port_count() const {
    return 2;
}

HoodieNode::PortType HNHoodieGeoVectorExpression::get_input_port_type(int p_port) const {
    switch (p_port) {
        case 0:
            return PortType::PORT_TYPE_HGEO;
        case 1:
            return PortType::PORT_TYPE_STRING;
        default:
            return PortType::PORT_TYPE_SCALAR;
    }
}

String HNHoodieGeoVectorExpression::get_input_port_name(int p_port) const {
    switch (p_port) {
        case 0:
            return "HGeo";
        case 1:
            return "Attribute";
        default:
            return "Data";
    }
}

int HNHoodieGeoVectorExpression::get_output_port_count() const {
    return 1;
}

HoodieNode::PortType HNHoodieGeoVectorExpression::get_output_port_type(int p_port) const {
    return PortType::PORT_TYPE_HGEO;
}

String HNHoodieGeoVectorExpression::get_output_port_name(int p_port) const {
    return "HGeo";
}

int HNHoodieGeoVectorExpression::get_property_input_count() const {
    return 3;
}

Variant::Type HNHoodieGeoVectorExpression::get_property_input_type(vec_size_t p_prop) const {
    switch (p_prop) {
        case 0:
            return Variant::STRING;
        case 1:
            return Variant::STRING;
        case 2:
            return Variant::INT;
        default:
            return Variant::INT;
    }
}

Variant HNHoodieGeoVectorExpression::get_property_input(vec_size_t p_port) const {
    switch (p_port) {
        case 0:
            return Variant(filter_expression);
        case 1:
            return Variant(expression);
        case 2:
            return Variant(target);
        default:
            return Variant();
    }
}

void HNHoodieGeoVectorExpression::set_property_input(vec_size_t p_prop, Variant p_input) {
    switch (p_prop) {
        case 0:
            filter_expression = (String)p_input;
            break;
        case 1:
            expression = (String)p_input;
            break;
        case 2:
            target = (Target)(int)p_input;
            break;
        default:
            return;
    }
}

Vector<StringName> HNHoodieGeoVectorExpression::get_editable_properties() const {
    Vector<StringName> props;
    props.push_back("Filter");
    props.push_back("Expression");
    props.push_back("Operation");
    return props;
}

HashMap<StringName, String> HNHoodieGeoVectorExpression::get_editable_properties_names() const {
    return HashMap<StringName, String>();
}

/*************************************************************************/
/*  csg_shape.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "csg_shape.h"

#include "core/math/geometry_2d.h"

void CSGShape3D::set_use_collision(bool p_enable) {
	if (use_collision == p_enable) {
		return;
	}

	use_collision = p_enable;

	if (!is_inside_tree() || !is_root_shape()) {
		return;
	}

	if (use_collision) {
		root_collision_shape.instantiate();
		root_collision_instance = PhysicsServer3D::get_singleton()->body_create();
		PhysicsServer3D::get_singleton()->body_set_mode(root_collision_instance, PhysicsServer3D::BODY_MODE_STATIC);
		PhysicsServer3D::get_singleton()->body_set_state(root_collision_instance, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
		PhysicsServer3D::get_singleton()->body_add_shape(root_collision_instance, root_collision_shape->get_rid());
		PhysicsServer3D::get_singleton()->body_set_space(root_collision_instance, get_world_3d()->get_space());
		PhysicsServer3D::get_singleton()->body_attach_object_instance_id(root_collision_instance, get_instance_id());
		set_collision_layer(collision_layer);
		set_collision_mask(collision_mask);
		set_collision_priority(collision_priority);
		_make_dirty(); //force update
	} else {
		PhysicsServer3D::get_singleton()->free(root_collision_instance);
		root_collision_instance = RID();
		root_collision_shape.unref();
	}
	notify_property_list_changed();
}

bool CSGShape3D::is_using_collision() const {
	return use_collision;
}

void CSGShape3D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	if (root_collision_instance.is_valid()) {
		PhysicsServer3D::get_singleton()->body_set_collision_layer(root_collision_instance, p_layer);
	}
}

uint32_t CSGShape3D::get_collision_layer() const {
	return collision_layer;
}

void CSGShape3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	if (root_collision_instance.is_valid()) {
		PhysicsServer3D::get_singleton()->body_set_collision_mask(root_collision_instance, p_mask);
	}
}

uint32_t CSGShape3D::get_collision_mask() const {
	return collision_mask;
}

void CSGShape3D::set_collision_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t layer = get_collision_layer();
	if (p_value) {
		layer |= 1 << (p_layer_number - 1);
	} else {
		layer &= ~(1 << (p_layer_number - 1));
	}
	set_collision_layer(layer);
}

bool CSGShape3D::get_collision_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_layer() & (1 << (p_layer_number - 1));
}

void CSGShape3D::set_collision_mask_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Collision layer number must be between 1 and 32 inclusive.");
	uint32_t mask = get_collision_mask();
	if (p_value) {
		mask |= 1 << (p_layer_number - 1);
	} else {
		mask &= ~(1 << (p_layer_number - 1));
	}
	set_collision_mask(mask);
}

bool CSGShape3D::get_collision_mask_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Collision layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Collision layer number must be between 1 and 32 inclusive.");
	return get_collision_mask() & (1 << (p_layer_number - 1));
}

void CSGShape3D::set_collision_priority(real_t p_priority) {
	collision_priority = p_priority;
	if (root_collision_instance.is_valid()) {
		PhysicsServer3D::get_singleton()->body_set_collision_priority(root_collision_instance, p_priority);
	}
}

real_t CSGShape3D::get_collision_priority() const {
	return collision_priority;
}

bool CSGShape3D::is_root_shape() const {
	return !parent_shape;
}

void CSGShape3D::set_snap(float p_snap) {
	snap = p_snap;
}

float CSGShape3D::get_snap() const {
	return snap;
}

void CSGShape3D::_make_dirty(bool p_parent_removing) {
	if ((p_parent_removing || is_root_shape()) && !dirty) {
		call_deferred(SNAME("_update_shape")); // Must be deferred; otherwise, is_root_shape() will use the previous parent
	}

	if (!is_root_shape()) {
		parent_shape->_make_dirty();
	} else if (!dirty) {
		call_deferred(SNAME("_update_shape"));
	}

	dirty = true;
}

CSGBrush CSGShape3D::_get_brush() {
	if (dirty) {
		csg_tool.instantiate();

		CSGBrush self_brush = _build_brush();
		csg_tool->add_brush(self_brush, static_cast<CSGTool::Operation>(operation), snap);

		for (int i = 0; i < get_child_count(); i++) {
			CSGShape3D *child = Object::cast_to<CSGShape3D>(get_child(i));
			if (!child) {
				continue;
			}
			if (!child->is_visible()) {
				continue;
			}

			CSGBrush child_brush = child->_get_brush();
			CSGBrush n;
			n.copy_from(child_brush, child->get_transform());
			csg_tool->add_brush(n, static_cast<CSGTool::Operation>(child->get_operation()), snap);
		}
		node_aabb = csg_tool->get_aabb();
		dirty = false;
	}

	return csg_tool->get_brush();
}

void CSGShape3D::_update_shape() {
	if (!is_root_shape()) {
		return;
	}

	set_base(RID());
	root_mesh.unref(); //byebye root mesh

	_get_brush();
	root_mesh = csg_tool->commit(Variant(), calculate_tangents);

	if (root_collision_shape.is_valid()) {
		Ref<ConcavePolygonShape3D> shape = csg_tool->create_trimesh_shape();
		root_collision_shape->set_faces(shape->get_faces());
	}

	set_base(root_mesh->get_rid());

	_update_collision_faces();
}

void CSGShape3D::_update_collision_faces() {
	if (use_collision && is_root_shape() && root_collision_shape.is_valid()) {
		CSGBrush *n = _get_brush();
		ERR_FAIL_COND_MSG(!n, "Cannot get CSGBrush.");
		Vector<Vector3> physics_faces;
		physics_faces.resize(n->faces.size() * 3);
		Vector3 *physicsw = physics_faces.ptrw();

		for (int i = 0; i < n->faces.size(); i++) {
			int order[3] = { 0, 1, 2 };

			if (n->faces[i].invert) {
				SWAP(order[1], order[2]);
			}

			physicsw[i * 3 + 0] = n->faces[i].vertices[order[0]];
			physicsw[i * 3 + 1] = n->faces[i].vertices[order[1]];
			physicsw[i * 3 + 2] = n->faces[i].vertices[order[2]];
		}

		root_collision_shape->set_faces(physics_faces);
	}
}

AABB CSGShape3D::get_aabb() const {
	return node_aabb;
}

Vector<Vector3> CSGShape3D::get_brush_faces() const {
	ERR_FAIL_COND_V(!is_inside_tree() || csg_tool.is_null(), Vector<Vector3>());
	return csg_tool->get_brush_faces();
}

void CSGShape3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			Node *parentn = get_parent();
			if (parentn) {
				parent_shape = Object::cast_to<CSGShape3D>(parentn);
				if (parent_shape) {
					set_base(RID());
					root_mesh.unref();
				}
			}
			if (!brush || parent_shape) {
				// Update this node if uninitialized, or both this node and its new parent if it gets added to another CSG shape
				_make_dirty();
			}
			last_visible = is_visible();
		} break;

		case NOTIFICATION_UNPARENTED: {
			if (!is_root_shape()) {
				// Update this node and its previous parent only if it's currently being removed from another CSG shape
				_make_dirty(true); // Must be forced since is_root_shape() uses the previous parent
			}
			parent_shape = nullptr;
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_root_shape() && last_visible != is_visible()) {
				// Update this node's parent only if its own visibility has changed, not the visibility of parent nodes
				parent_shape->_make_dirty();
			}
			last_visible = is_visible();
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (!is_root_shape()) {
				// Update this node's parent only if its own transformation has changed, not the transformation of parent nodes
				parent_shape->_make_dirty();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (use_collision && is_root_shape()) {
				root_collision_shape.instantiate();
				root_collision_instance = PhysicsServer3D::get_singleton()->body_create();
				PhysicsServer3D::get_singleton()->body_set_mode(root_collision_instance, PhysicsServer3D::BODY_MODE_STATIC);
				PhysicsServer3D::get_singleton()->body_set_state(root_collision_instance, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
				PhysicsServer3D::get_singleton()->body_add_shape(root_collision_instance, root_collision_shape->get_rid());
				PhysicsServer3D::get_singleton()->body_set_space(root_collision_instance, get_world_3d()->get_space());
				PhysicsServer3D::get_singleton()->body_attach_object_instance_id(root_collision_instance, get_instance_id());
				set_collision_layer(collision_layer);
				set_collision_mask(collision_mask);
				set_collision_priority(collision_priority);
				_update_collision_faces();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (use_collision && is_root_shape() && root_collision_instance.is_valid()) {
				PhysicsServer3D::get_singleton()->free(root_collision_instance);
				root_collision_instance = RID();
				root_collision_shape.unref();
			}
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (use_collision && is_root_shape() && root_collision_instance.is_valid()) {
				PhysicsServer3D::get_singleton()->body_set_state(root_collision_instance, PhysicsServer3D::BODY_STATE_TRANSFORM, get_global_transform());
			}
		} break;
	}
}

void CSGShape3D::set_operation(Operation p_operation) {
	operation = p_operation;
	_make_dirty();
	update_gizmos();
}

CSGShape3D::Operation CSGShape3D::get_operation() const {
	return operation;
}

void CSGShape3D::set_calculate_tangents(bool p_calculate_tangents) {
	calculate_tangents = p_calculate_tangents;
	_make_dirty();
}

bool CSGShape3D::is_calculating_tangents() const {
	return calculate_tangents;
}

void CSGShape3D::_validate_property(PropertyInfo &p_property) const {
	bool is_collision_prefixed = p_property.name.begins_with("collision_");
	if ((is_collision_prefixed || p_property.name.begins_with("use_collision")) && is_inside_tree() && !is_root_shape()) {
		//hide collision if not root
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	} else if (is_collision_prefixed && !bool(get("use_collision"))) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

Array CSGShape3D::get_meshes() const {
	if (root_mesh.is_valid()) {
		Array arr;
		arr.resize(2);
		arr[0] = Transform3D();
		arr[1] = root_mesh;
		return arr;
	}

	return Array();
}

void CSGShape3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_shape"), &CSGShape3D::_update_shape);
	ClassDB::bind_method(D_METHOD("is_root_shape"), &CSGShape3D::is_root_shape);

	ClassDB::bind_method(D_METHOD("set_operation", "operation"), &CSGShape3D::set_operation);
	ClassDB::bind_method(D_METHOD("get_operation"), &CSGShape3D::get_operation);

	ClassDB::bind_method(D_METHOD("set_snap", "snap"), &CSGShape3D::set_snap);
	ClassDB::bind_method(D_METHOD("get_snap"), &CSGShape3D::get_snap);

	ClassDB::bind_method(D_METHOD("set_use_collision", "operation"), &CSGShape3D::set_use_collision);
	ClassDB::bind_method(D_METHOD("is_using_collision"), &CSGShape3D::is_using_collision);

	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &CSGShape3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &CSGShape3D::get_collision_layer);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &CSGShape3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &CSGShape3D::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_mask_value", "layer_number", "value"), &CSGShape3D::set_collision_mask_value);
	ClassDB::bind_method(D_METHOD("get_collision_mask_value", "layer_number"), &CSGShape3D::get_collision_mask_value);

	ClassDB::bind_method(D_METHOD("set_collision_layer_value", "layer_number", "value"), &CSGShape3D::set_collision_layer_value);
	ClassDB::bind_method(D_METHOD("get_collision_layer_value", "layer_number"), &CSGShape3D::get_collision_layer_value);

	ClassDB::bind_method(D_METHOD("set_collision_priority", "priority"), &CSGShape3D::set_collision_priority);
	ClassDB::bind_method(D_METHOD("get_collision_priority"), &CSGShape3D::get_collision_priority);

	ClassDB::bind_method(D_METHOD("set_calculate_tangents", "enabled"), &CSGShape3D::set_calculate_tangents);
	ClassDB::bind_method(D_METHOD("is_calculating_tangents"), &CSGShape3D::is_calculating_tangents);

	ClassDB::bind_method(D_METHOD("get_meshes"), &CSGShape3D::get_meshes);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operation", PROPERTY_HINT_ENUM, "Union,Intersection,Subtraction"), "set_operation", "get_operation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "snap", PROPERTY_HINT_RANGE, "0.0001,1,0.001,suffix:m"), "set_snap", "get_snap");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "calculate_tangents"), "set_calculate_tangents", "is_calculating_tangents");

	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_collision"), "set_use_collision", "is_using_collision");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_priority"), "set_collision_priority", "get_collision_priority");

	BIND_ENUM_CONSTANT(OPERATION_UNION);
	BIND_ENUM_CONSTANT(OPERATION_INTERSECTION);
	BIND_ENUM_CONSTANT(OPERATION_SUBTRACTION);
}

CSGShape3D::CSGShape3D() {
	csg_tool.instantiate();
	set_notify_local_transform(true);
}

//////////////////////////////////

CSGBrush CSGCombiner3D::_build_brush() {
	return CSGBrush(); //does not build anything
}

CSGCombiner3D::CSGCombiner3D() {
}

/////////////////////

CSGBrush *CSGPrimitive3D::_create_brush_from_arrays(const Vector<Vector3> &p_vertices, const Vector<Vector2> &p_uv, const Vector<bool> &p_smooth, const Vector<Ref<Material>> &p_materials) {
	CSGBrush *new_brush = memnew(CSGBrush);

	Vector<bool> invert;
	invert.resize(p_vertices.size() / 3);
	{
		int ic = invert.size();
		bool *w = invert.ptrw();
		for (int i = 0; i < ic; i++) {
			w[i] = flip_faces;
		}
	}
	new_brush->build_from_faces(p_vertices, p_uv, p_smooth, p_materials, invert);

	return new_brush;
}

void CSGPrimitive3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_flip_faces", "flip_faces"), &CSGPrimitive3D::set_flip_faces);
	ClassDB::bind_method(D_METHOD("get_flip_faces"), &CSGPrimitive3D::get_flip_faces);
	ClassDB::bind_method(D_METHOD("set_smooth_faces", "smooth_faces"), &CSGPrimitive3D::set_smooth_faces);
	ClassDB::bind_method(D_METHOD("get_smooth_faces"), &CSGPrimitive3D::get_smooth_faces);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &CSGPrimitive3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CSGPrimitive3D::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_faces"), "set_flip_faces", "get_flip_faces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smooth_faces"), "set_smooth_faces", "get_smooth_faces");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material", "get_material");
}

void CSGPrimitive3D::set_flip_faces(bool p_invert) {
	Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	csg_primitive->set_invert_faces(p_invert);
	_make_dirty();
}

bool CSGPrimitive3D::get_flip_faces() const {
	const Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	return csg_primitive->get_flip_faces();
}

void CSGPrimitive3D::set_smooth_faces(bool p_smooth_faces) {
	Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	csg_primitive->set_smooth_faces(p_smooth_faces);
	_make_dirty();
}

bool CSGPrimitive3D::get_flip_faces() {
	return flip_faces;
}
bool CSGPrimitive3D::get_smooth_faces() const {
	const Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	return csg_primitive->get_smooth_faces();
}

CSGPrimitive3D::CSGPrimitive3D() {
	flip_faces = false;
}
void CSGPrimitive3D::set_material(const Ref<Material> &p_material) {
	Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	csg_primitive->set_material(p_material);
	_make_dirty();
}

Ref<Material> CSGPrimitive3D::get_material() const {
	const Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	return csg_primitive->get_material();
}

CSGBrush CSGPrimitive3D::_build_brush() {
	Ref<CSGPrimitiveShape3D> csg_primitive = get_csg_primitive();
	if (csg_primitive.is_null()) {
		return CSGBrush();
	} else {
		CSGBrush brush = csg_primitive->get_brush();
		return brush;
	}
}

/////////////////////

void CSGMesh3D::_mesh_changed() {
	_make_dirty();
	update_gizmos();
}

void CSGMesh3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &CSGMesh3D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &CSGMesh3D::get_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
}

void CSGMesh3D::set_mesh(const Ref<Mesh> &p_mesh) {
	Ref<Mesh> mesh = csg_primitive->get_mesh();
	if (mesh == p_mesh) {
		return;
	}
	if (mesh.is_valid()) {
		mesh->disconnect("changed", callable_mp(this, &CSGMesh3D::_mesh_changed));
	}
	csg_primitive->set_mesh(p_mesh);
	mesh = csg_primitive->get_mesh();

	if (mesh.is_valid()) {
		mesh->connect("changed", callable_mp(this, &CSGMesh3D::_mesh_changed));
	}

	_mesh_changed();
}

Ref<Mesh> CSGMesh3D::get_mesh() const {
	return csg_primitive->get_mesh();
}

CSGMesh3D::CSGMesh3D() {
	csg_primitive.instantiate();
}

////////////////////////////////

void CSGSphere3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CSGSphere3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CSGSphere3D::get_radius);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "radial_segments"), &CSGSphere3D::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CSGSphere3D::get_radial_segments);
	ClassDB::bind_method(D_METHOD("set_rings", "rings"), &CSGSphere3D::set_rings);
	ClassDB::bind_method(D_METHOD("get_rings"), &CSGSphere3D::get_rings);

	ClassDB::bind_method(D_METHOD("set_smooth_faces", "smooth_faces"), &CSGSphere3D::set_smooth_faces);
	ClassDB::bind_method(D_METHOD("get_smooth_faces"), &CSGSphere3D::get_smooth_faces);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &CSGSphere3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CSGSphere3D::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,100.0,0.001,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1"), "set_radial_segments", "get_radial_segments");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rings", PROPERTY_HINT_RANGE, "1,100,1"), "set_rings", "get_rings");
}

void CSGSphere3D::set_radius(const float p_radius) {
	csg_primitive->set_radius(p_radius);
	_make_dirty();
	update_gizmos();
}

float CSGSphere3D::get_radius() const {
	return csg_primitive->get_radius();
}

void CSGSphere3D::set_radial_segments(const int p_radial_segments) {
	csg_primitive->set_radial_segments(p_radial_segments);
	_make_dirty();
	update_gizmos();
}

int CSGSphere3D::get_radial_segments() const {
	return csg_primitive->get_radial_segments();
}

void CSGSphere3D::set_rings(const int p_rings) {
	csg_primitive->set_rings(p_rings);
	_make_dirty();
	update_gizmos();
}

int CSGSphere3D::get_rings() const {
	return csg_primitive->get_rings();
}

CSGSphere3D::CSGSphere3D() {
	// defaults
	radius = 0.5;
	radial_segments = 12;
	rings = 6;
	smooth_faces = true;
	csg_primitive.instantiate();
}

///////////////

void CSGBox3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &CSGBox3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &CSGBox3D::get_size);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &CSGBox3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CSGBox3D::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material", "get_material");
}

void CSGBox3D::set_size(const Vector3 &p_size) {
	csg_primitive->set_size(p_size);
	_make_dirty();
	update_gizmos();
}

Vector3 CSGBox3D::get_size() const {
	return csg_primitive->get_size();
}

CSGBox3D::CSGBox3D() {
	csg_primitive.instantiate();
}

///////////////

void CSGCylinder3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CSGCylinder3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CSGCylinder3D::get_radius);

	ClassDB::bind_method(D_METHOD("set_height", "height"), &CSGCylinder3D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CSGCylinder3D::get_height);

	ClassDB::bind_method(D_METHOD("set_sides", "sides"), &CSGCylinder3D::set_sides);
	ClassDB::bind_method(D_METHOD("get_sides"), &CSGCylinder3D::get_sides);

	ClassDB::bind_method(D_METHOD("set_cone", "cone"), &CSGCylinder3D::set_cone);
	ClassDB::bind_method(D_METHOD("is_cone"), &CSGCylinder3D::is_cone);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &CSGCylinder3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CSGCylinder3D::get_material);

	ClassDB::bind_method(D_METHOD("set_smooth_faces", "smooth_faces"), &CSGCylinder3D::set_smooth_faces);
	ClassDB::bind_method(D_METHOD("get_smooth_faces"), &CSGCylinder3D::get_smooth_faces);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sides", PROPERTY_HINT_RANGE, "3,64,1"), "set_sides", "get_sides");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cone"), "set_cone", "is_cone");
}

void CSGCylinder3D::set_radius(const float p_radius) {
	csg_primitive->set_radius(p_radius);
	_make_dirty();
	update_gizmos();
}

float CSGCylinder3D::get_radius() const {
	return csg_primitive->get_radius();
}

void CSGCylinder3D::set_height(const float p_height) {
	csg_primitive->set_height(p_height);
	_make_dirty();
	update_gizmos();
}

float CSGCylinder3D::get_height() const {
	return csg_primitive->get_height();
}

void CSGCylinder3D::set_sides(const int p_sides) {
	csg_primitive->set_sides(p_sides);
	_make_dirty();
	update_gizmos();
}

int CSGCylinder3D::get_sides() const {
	return csg_primitive->get_sides();
}

void CSGCylinder3D::set_cone(const bool p_cone) {
	csg_primitive->set_cone(p_cone);
	_make_dirty();
	update_gizmos();
}

bool CSGCylinder3D::is_cone() const {
	return csg_primitive->is_cone();
}

CSGCylinder3D::CSGCylinder3D() {
	// defaults
	radius = 0.5;
	height = 2.0;
	sides = 8;
	cone = false;
	smooth_faces = true;
	csg_primitive.instantiate();
}

////////////

void CSGTorus3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_inner_radius", "radius"), &CSGTorus3D::set_inner_radius);
	ClassDB::bind_method(D_METHOD("get_inner_radius"), &CSGTorus3D::get_inner_radius);

	ClassDB::bind_method(D_METHOD("set_outer_radius", "radius"), &CSGTorus3D::set_outer_radius);
	ClassDB::bind_method(D_METHOD("get_outer_radius"), &CSGTorus3D::get_outer_radius);

	ClassDB::bind_method(D_METHOD("set_sides", "sides"), &CSGTorus3D::set_sides);
	ClassDB::bind_method(D_METHOD("get_sides"), &CSGTorus3D::get_sides);
	ClassDB::bind_method(D_METHOD("set_ring_sides", "ring_sides"), &CSGTorus3D::set_ring_sides);
	ClassDB::bind_method(D_METHOD("get_ring_sides"), &CSGTorus3D::get_ring_sides);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &CSGTorus3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CSGTorus3D::get_material);

	ClassDB::bind_method(D_METHOD("set_smooth_faces", "smooth_faces"), &CSGTorus3D::set_smooth_faces);
	ClassDB::bind_method(D_METHOD("get_smooth_faces"), &CSGTorus3D::get_smooth_faces);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inner_radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_inner_radius", "get_inner_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "outer_radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_outer_radius", "get_outer_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sides", PROPERTY_HINT_RANGE, "3,64,1"), "set_sides", "get_sides");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ring_sides", PROPERTY_HINT_RANGE, "3,64,1"), "set_ring_sides", "get_ring_sides");
}

void CSGTorus3D::set_inner_radius(const float p_inner_radius) {
	csg_primitive->set_inner_radius(p_inner_radius);
	_make_dirty();
	update_gizmos();
}

float CSGTorus3D::get_inner_radius() const {
	return csg_primitive->get_inner_radius();
}

void CSGTorus3D::set_outer_radius(const float p_outer_radius) {
	csg_primitive->set_outer_radius(p_outer_radius);
	_make_dirty();
	update_gizmos();
}

float CSGTorus3D::get_outer_radius() const {
	return csg_primitive->get_outer_radius();
}

void CSGTorus3D::set_sides(const int p_sides) {
	csg_primitive->set_sides(p_sides);
	_make_dirty();
	update_gizmos();
}

int CSGTorus3D::get_sides() const {
	return csg_primitive->get_sides();
}

void CSGTorus3D::set_ring_sides(const int p_ring_sides) {
	csg_primitive->set_ring_sides(p_ring_sides);
	_make_dirty();
	update_gizmos();
}

int CSGTorus3D::get_ring_sides() const {
	return csg_primitive->get_ring_sides();
}

CSGTorus3D::CSGTorus3D() {
	// defaults
	inner_radius = 0.5;
	outer_radius = 1.0;
	sides = 8;
	ring_sides = 6;
	smooth_faces = true;
	csg_primitive.instantiate();
}

///////////////

CSGBrush CSGPolygon3D::_build_brush() {
	// set our bounding box

	if (csg_primitive->get_mode() == CSGPolygonShape3D::MODE_PATH) {
		if (!has_node(path_node)) {
			return CSGBrush();
		}
		Node *n = get_node(path_node);
		if (!n) {
			return CSGBrush();
		}
		Path3D *path = Object::cast_to<Path3D>(n);
		if (!path) {
			return CSGBrush();
		}

		Ref<Curve3D> curve = path->get_curve();
		if (curve.is_null() || curve->get_point_count() < 2) {
			return CSGBrush();
		}
		if (curve->get_baked_length() <= 0) {
			return CSGBrush();
		}
		csg_primitive->set_path_curve(curve);

		Transform3D path_to_this;
		if (!path_local) {
			// center on paths origin
			path_to_this = get_global_transform().affine_inverse() * path->get_global_transform();
		}
		csg_primitive->set_path_transform(path_to_this);
	}

	return CSGPrimitive3D::_build_brush();
}

void CSGPolygon3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (path) {
			path->disconnect("tree_exited", callable_mp(this, &CSGPolygon3D::_path_exited));
			path->disconnect("curve_changed", callable_mp(this, &CSGPolygon3D::_path_changed));
			path = nullptr;
		}
	}
}

void CSGPolygon3D::_validate_property(PropertyInfo &p_property) const {
	CSGPolygonShape3D::Mode mode = csg_primitive->get_mode();
	if (p_property.name.begins_with("spin") && mode != CSGPolygonShape3D::MODE_SPIN) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
	if (p_property.name.begins_with("path") && mode != CSGPolygonShape3D::MODE_PATH) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
	if (p_property.name == "depth" && mode != CSGPolygonShape3D::MODE_DEPTH) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void CSGPolygon3D::_path_changed() {
	_make_dirty();
	update_gizmos();
}

void CSGPolygon3D::_path_exited() {
	path = nullptr;
}

void CSGPolygon3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &CSGPolygon3D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &CSGPolygon3D::get_polygon);

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &CSGPolygon3D::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &CSGPolygon3D::get_mode);

	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &CSGPolygon3D::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &CSGPolygon3D::get_depth);

	ClassDB::bind_method(D_METHOD("set_spin_degrees", "degrees"), &CSGPolygon3D::set_spin_degrees);
	ClassDB::bind_method(D_METHOD("get_spin_degrees"), &CSGPolygon3D::get_spin_degrees);

	ClassDB::bind_method(D_METHOD("set_spin_sides", "spin_sides"), &CSGPolygon3D::set_spin_sides);
	ClassDB::bind_method(D_METHOD("get_spin_sides"), &CSGPolygon3D::get_spin_sides);

	ClassDB::bind_method(D_METHOD("set_path_node", "path"), &CSGPolygon3D::set_path_node);
	ClassDB::bind_method(D_METHOD("get_path_node"), &CSGPolygon3D::get_path_node);

	ClassDB::bind_method(D_METHOD("set_path_interval_type", "interval_type"), &CSGPolygon3D::set_path_interval_type);
	ClassDB::bind_method(D_METHOD("get_path_interval_type"), &CSGPolygon3D::get_path_interval_type);

	ClassDB::bind_method(D_METHOD("set_path_interval", "interval"), &CSGPolygon3D::set_path_interval);
	ClassDB::bind_method(D_METHOD("get_path_interval"), &CSGPolygon3D::get_path_interval);

	ClassDB::bind_method(D_METHOD("set_path_simplify_angle", "degrees"), &CSGPolygon3D::set_path_simplify_angle);
	ClassDB::bind_method(D_METHOD("get_path_simplify_angle"), &CSGPolygon3D::get_path_simplify_angle);

	ClassDB::bind_method(D_METHOD("set_path_rotation", "path_rotation"), &CSGPolygon3D::set_path_rotation);
	ClassDB::bind_method(D_METHOD("get_path_rotation"), &CSGPolygon3D::get_path_rotation);

	ClassDB::bind_method(D_METHOD("set_path_local", "enable"), &CSGPolygon3D::set_path_local);
	ClassDB::bind_method(D_METHOD("is_path_local"), &CSGPolygon3D::is_path_local);

	ClassDB::bind_method(D_METHOD("set_path_continuous_u", "enable"), &CSGPolygon3D::set_path_continuous_u);
	ClassDB::bind_method(D_METHOD("is_path_continuous_u"), &CSGPolygon3D::is_path_continuous_u);

	ClassDB::bind_method(D_METHOD("set_path_u_distance", "distance"), &CSGPolygon3D::set_path_u_distance);
	ClassDB::bind_method(D_METHOD("get_path_u_distance"), &CSGPolygon3D::get_path_u_distance);

	ClassDB::bind_method(D_METHOD("set_path_joined", "enable"), &CSGPolygon3D::set_path_joined);
	ClassDB::bind_method(D_METHOD("is_path_joined"), &CSGPolygon3D::is_path_joined);

	ClassDB::bind_method(D_METHOD("_is_editable_3d_polygon"), &CSGPolygon3D::_is_editable_3d_polygon);
	ClassDB::bind_method(D_METHOD("_has_editable_3d_polygon_no_depth"), &CSGPolygon3D::_has_editable_3d_polygon_no_depth);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Depth,Spin,Path"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth", PROPERTY_HINT_RANGE, "0.01,100.0,0.01,or_greater,exp,suffix:m"), "set_depth", "get_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spin_degrees", PROPERTY_HINT_RANGE, "1,360,0.1"), "set_spin_degrees", "get_spin_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "spin_sides", PROPERTY_HINT_RANGE, "3,64,1"), "set_spin_sides", "get_spin_sides");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "path_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Path3D"), "set_path_node", "get_path_node");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_interval_type", PROPERTY_HINT_ENUM, "Distance,Subdivide"), "set_path_interval_type", "get_path_interval_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_interval", PROPERTY_HINT_RANGE, "0.01,1.0,0.01,exp,or_greater"), "set_path_interval", "get_path_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_simplify_angle", PROPERTY_HINT_RANGE, "0.0,180.0,0.1,exp"), "set_path_simplify_angle", "get_path_simplify_angle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_rotation", PROPERTY_HINT_ENUM, "Polygon,Path,PathFollow"), "set_path_rotation", "get_path_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "path_local"), "set_path_local", "is_path_local");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "path_continuous_u"), "set_path_continuous_u", "is_path_continuous_u");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_u_distance", PROPERTY_HINT_RANGE, "0.0,10.0,0.01,or_greater,suffix:m"), "set_path_u_distance", "get_path_u_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "path_joined"), "set_path_joined", "is_path_joined");

	BIND_ENUM_CONSTANT(MODE_DEPTH);
	BIND_ENUM_CONSTANT(MODE_SPIN);
	BIND_ENUM_CONSTANT(MODE_PATH);

	BIND_ENUM_CONSTANT(PATH_ROTATION_POLYGON);
	BIND_ENUM_CONSTANT(PATH_ROTATION_PATH);
	BIND_ENUM_CONSTANT(PATH_ROTATION_PATH_FOLLOW);

	BIND_ENUM_CONSTANT(PATH_INTERVAL_DISTANCE);
	BIND_ENUM_CONSTANT(PATH_INTERVAL_SUBDIVIDE);
}

void CSGPolygon3D::set_polygon(const Vector<Vector2> &p_polygon) {
	csg_primitive->set_polygon(p_polygon);
	_make_dirty();
	update_gizmos();
}

Vector<Vector2> CSGPolygon3D::get_polygon() const {
	return csg_primitive->get_polygon();
}

void CSGPolygon3D::set_mode(Mode p_mode) {
	csg_primitive->set_mode(static_cast<CSGPolygonShape3D::Mode>(p_mode));
	_make_dirty();
	update_gizmos();
	notify_property_list_changed();
}

CSGPolygon3D::Mode CSGPolygon3D::get_mode() const {
	return static_cast<CSGPolygon3D::Mode>(csg_primitive->get_mode());
}

void CSGPolygon3D::set_depth(const float p_depth) {
	csg_primitive->set_depth(p_depth);
	_make_dirty();
	update_gizmos();
}

float CSGPolygon3D::get_depth() const {
	return csg_primitive->get_depth();
}

void CSGPolygon3D::set_path_continuous_u(bool p_enable) {
	csg_primitive->set_path_continuous_u(p_enable);
	_make_dirty();
}

bool CSGPolygon3D::is_path_continuous_u() const {
	return csg_primitive->is_path_continuous_u();
}

void CSGPolygon3D::set_path_u_distance(real_t p_path_u_distance) {
	path_u_distance = p_path_u_distance;
	_make_dirty();
	update_gizmos();
}

real_t CSGPolygon3D::get_path_u_distance() const {
	return path_u_distance;
}

void CSGPolygon3D::set_spin_degrees(const float p_spin_degrees) {
	csg_primitive->set_spin_degrees(p_spin_degrees);
	_make_dirty();
	update_gizmos();
}

float CSGPolygon3D::get_spin_degrees() const {
	return csg_primitive->get_spin_degrees();
}

void CSGPolygon3D::set_spin_sides(int p_spin_sides) {
	csg_primitive->set_spin_sides(p_spin_sides);
	_make_dirty();
	update_gizmos();
}

int CSGPolygon3D::get_spin_sides() const {
	return csg_primitive->get_spin_sides();
}

void CSGPolygon3D::set_path_node(const NodePath &p_path) {
	path_node = p_path;
	_make_dirty();
	update_gizmos();
}

NodePath CSGPolygon3D::get_path_node() const {
	return path_node;
}

void CSGPolygon3D::set_path_interval_type(PathIntervalType p_interval_type) {
	path_interval_type = p_interval_type;
	_make_dirty();
	update_gizmos();
}

CSGPolygon3D::PathIntervalType CSGPolygon3D::get_path_interval_type() const {
	return path_interval_type;
}

void CSGPolygon3D::set_path_interval(float p_interval) {
	csg_primitive->set_path_interval(p_interval);
	_make_dirty();
	update_gizmos();
}

float CSGPolygon3D::get_path_interval() const {
	return csg_primitive->get_path_interval();
}

void CSGPolygon3D::set_path_simplify_angle(float p_angle) {
	path_simplify_angle = p_angle;
	_make_dirty();
	update_gizmos();
}

float CSGPolygon3D::get_path_simplify_angle() const {
	return path_simplify_angle;
}

void CSGPolygon3D::set_path_rotation(CSGPolygon3D::PathRotation p_rotation) {
	csg_primitive->set_path_rotation(static_cast<CSGPolygonShape3D::PathRotation>(p_rotation));
	_make_dirty();
	update_gizmos();
}

CSGPolygon3D::PathRotation CSGPolygon3D::get_path_rotation() const {
	return static_cast<CSGPolygon3D::PathRotation>(csg_primitive->get_path_rotation());
}

void CSGPolygon3D::set_path_local(bool p_enable) {
	path_local = p_enable;
	_make_dirty();
	update_gizmos();
}

bool CSGPolygon3D::is_path_local() const {
	return path_local;
}

void CSGPolygon3D::set_path_joined(bool p_enable) {
	csg_primitive->set_path_joined(p_enable);
	_make_dirty();
	update_gizmos();
}

bool CSGPolygon3D::is_path_joined() const {
	return csg_primitive->is_path_joined();
}

bool CSGPolygon3D::_is_editable_3d_polygon() const {
	return true;
}

bool CSGPolygon3D::_has_editable_3d_polygon_no_depth() const {
	return true;
}

CSGPolygon3D::CSGPolygon3D() {
	csg_primitive.instantiate();
	Vector<Vector2> polygon;
	polygon.push_back(Vector2(0, 0));
	polygon.push_back(Vector2(0, 1));
	polygon.push_back(Vector2(1, 1));
	polygon.push_back(Vector2(1, 0));
	depth = 1.0;
	spin_degrees = 360;
	spin_sides = 8;
	smooth_faces = false;
	path_interval_type = PATH_INTERVAL_DISTANCE;
	path_interval = 1.0;
	path_simplify_angle = 0.0;
	path_rotation = PATH_ROTATION_PATH_FOLLOW;
	path_local = false;
	path_continuous_u = true;
	path_u_distance = 1.0;
	path_joined = false;
	path = nullptr;
	set_polygon(polygon);
}

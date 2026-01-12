//
// Created by niooi on 10/13/2025.
//

#pragma once

#include <prelude.h>
#include "glm/ext/matrix_clip_space.hpp"

namespace v {
    class Camera : public Domain<Camera> {
    public:
        Camera(f32 fov = 90, f32 aspect = 1.7777f, f32 near = 0.01f, f32 far = 1000.f) :
            Domain(), fov_(fov), aspect_(aspect), near_(near), far_(far), pitch_(0.0f),
            yaw_(0.0f), roll_(0.0f)
        {}

        void init() override
        {
            attach<Rotation>();
            attach<Pos3d>();

            recalc_static_matrices();
            rebuild_rotation();
        }

        /// Returns the view-projection matrix of the camera
        FORCEINLINE glm::mat4 matrix()
        {
            auto& pos = get<Pos3d>().val;
            auto& rot = get<Rotation>().val;

            // View matrix: inverse of camera transform
            glm::mat4 view =
                glm::translate(glm::mat4(1.0f), -pos) * glm::transpose(glm::toMat4(rot));

            return perspective_ * view;
        }

        /// Get the view matrix (without projection)
        FORCEINLINE glm::mat4 view_matrix()
        {
            auto& pos = get<Pos3d>().val;
            auto& rot = get<Rotation>().val;

            return glm::translate(glm::mat4(1.0f), -pos) *
                glm::transpose(glm::toMat4(rot));
        }

        /// Set pitch (rotation around X axis) in radians
        FORCEINLINE void set_pitch(f32 angle)
        {
            pitch_ = angle;
            rebuild_rotation();
        }

        /// Add to pitch (rotation around X axis) in radians
        FORCEINLINE void add_pitch(f32 angle)
        {
            pitch_ += angle;
            rebuild_rotation();
        }

        /// Set yaw (rotation around Y axis) in radians
        FORCEINLINE void set_yaw(f32 angle)
        {
            yaw_ = angle;
            rebuild_rotation();
        }

        /// Add to yaw (rotation around Y axis) in radians
        FORCEINLINE void add_yaw(f32 angle)
        {
            yaw_ += angle;
            rebuild_rotation();
        }

        /// Set roll (rotation around Z axis) in radians
        FORCEINLINE void set_roll(f32 angle)
        {
            roll_ = angle;
            rebuild_rotation();
        }

        /// Add to roll (rotation around Z axis) in radians
        FORCEINLINE void add_roll(f32 angle)
        {
            roll_ += angle;
            rebuild_rotation();
        }

        /// Get current pitch in radians
        FORCEINLINE f32 pitch() const { return pitch_; }

        /// Get current yaw in radians
        FORCEINLINE f32 yaw() const { return yaw_; }

        /// Get current roll in radians
        FORCEINLINE f32 roll() const { return roll_; }

        /// Get the camera's forward direction vector
        FORCEINLINE glm::vec3 forward() const
        {
            auto& rot = get<Rotation>().val;
            return rot * glm::vec3(0.0f, 0.0f, -1.0f);
        }

        /// Get the camera's right direction vector
        FORCEINLINE glm::vec3 right() const
        {
            auto& rot = get<Rotation>().val;
            return rot * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        /// Get the camera's up direction vector
        FORCEINLINE glm::vec3 up() const
        {
            auto& rot = get<Rotation>().val;
            return rot * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        /// Get the field of view in radians
        FORCEINLINE f32 fov() const { return fov_; }

        /// Set the field of view (in radians) and recalculate perspective matrix
        FORCEINLINE void fov(f32 new_fov)
        {
            fov_ = new_fov;
            recalc_static_matrices();
        }

        /// Get the aspect ratio
        FORCEINLINE f32 aspect() const { return aspect_; }

        /// Set the aspect ratio and recalculate perspective matrix
        FORCEINLINE void set_aspect(f32 new_aspect)
        {
            aspect_ = new_aspect;
            recalc_static_matrices();
        }

        /// Get the near plane distance
        FORCEINLINE f32 near_plane() const { return near_; }

        /// Set the near plane distance and recalculate perspective matrix
        FORCEINLINE void set_near(f32 new_near)
        {
            near_ = new_near;
            recalc_static_matrices();
        }

        /// Get the far plane distance
        FORCEINLINE f32 far_plane() const { return far_; }

        /// Set the far plane distance and recalculate perspective matrix
        FORCEINLINE void set_far(f32 new_far)
        {
            far_ = new_far;
            recalc_static_matrices();
        }

        /// Set all perspective parameters at once
        FORCEINLINE void
        set_perspective(f32 new_fov, f32 new_aspect, f32 new_near, f32 new_far)
        {
            fov_    = new_fov;
            aspect_ = new_aspect;
            near_   = new_near;
            far_    = new_far;
            recalc_static_matrices();
        }

        /// Get the perspective matrix
        FORCEINLINE const glm::mat4& perspective() const { return perspective_; }

    private:
        f32 fov_, aspect_, near_, far_;
        f32 pitch_, yaw_, roll_;

        glm::mat4 perspective_;

        // recalculates persp matrix bc it doesnt change a lot i'd hope
        FORCEINLINE void recalc_static_matrices()
        {
            perspective_ = glm::perspective(fov_, aspect_, near_, far_);
        }

        // rebuilds quaternion from Euler angles
        FORCEINLINE void rebuild_rotation()
        {
            auto&     rot        = get<Rotation>().val;
            glm::quat quat_yaw   = glm::angleAxis(yaw_, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat quat_pitch = glm::angleAxis(pitch_, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat quat_roll  = glm::angleAxis(roll_, glm::vec3(0.0f, 0.0f, 1.0f));
            rot                  = quat_yaw * quat_pitch * quat_roll;
        }
    };
} // namespace v

// Simple OpenSCAD model for Morphing Star Hybrid Quad-Glider Concept
// Central body + 4 deployable arms with rods and placeholder fabric

$fn = 50;

// Parameters (scale in mm - adjust for your size)
body_size = 200;      // Central fuselage
arm_length = 800;     // Deployed arm length
       // Carbon rod diameter
motor_mount_diam = 50;
rod_diam = 30;         // Carbon rod diameter
rod_spacing = 40;     // Distance between rods on the same side
// Central Body (box for simplicity) with component mounts
module central_body() {
    cube([body_size, body_size, 80], center=true);
    
    // 2x Redundant Raspberry Pi mounts (side by side)
    translate([-body_size/2 + 40, 30, 10]) cube([85, 56, 5]);  // Pi board footprint approx
    translate([-body_size/2 + 40, -90, 10]) cube([85, 56, 5]);
    
    // Jetson Orin Nano NX mount (larger)
    translate([body_size/2 - 120, 0, 10]) cube([100, 80, 5]);  // Approximate Orin footprint
}

// Single Arm with rods (simplified cruciform)
module arm(angle) {
    rotate([0,0,angle]) {
        // Main arm spar
        translate([body_size/2, 0, 0]) cube([arm_length, rod_diam, rod_diam], center=false);
            
// Secondary rods for star/membrane support - 3 per side (thinner as requested)
        // Positive Y side (outer side)
        translate([body_size/2,  rod_spacing*1, rod_spacing/2]) cube([arm_length, rod_diam/4, rod_diam/4]); // farthest
        translate([body_size/2,  rod_spacing*1, 0]) cube([arm_length, rod_diam/4, rod_diam/4]); // middle
        //translate([body_size/2,  rod_spacing*0.2, 0]) cube([arm_length, rod_diam/4, rod_diam/4]); // closest
        
        // Negative Y side (inner side)
        //translate([body_size/2, -rod_spacing*0.2, 0]) cube([arm_length, rod_diam/4, rod_diam/4]); // closest
        translate([body_size/2, -rod_spacing*1, rod_spacing/2]) cube([arm_length, rod_diam/4, rod_diam/4]); // middle
        translate([body_size/2, -rod_spacing*1, 0]) cube([arm_length, rod_diam/4, rod_diam/4]); // farthestfarther out
        
        // Secondary rods for star/membrane support
// Secondary rods for star/membrane support - 2 per side
        // Positive Y side (outer side)
//translate([body_size/2, 50, 0]) cube([arm_length, rod_diam/4, rod_diam/4]);
  //      translate([body_size/2, -50, 0]) cube([arm_length, rod_diam/4, rod_diam/4]);
        
        // XRotor motor mount at tip (with bolt pattern placeholder)
        translate([body_size/2 + arm_length, 0, 30]) {
            cylinder(h=40, d=motor_mount_diam);  // Main mount cylinder
            // Bolt holes for XRotor (typical 4x M4 pattern)
            for (bolt_ang = [0:90:270]) {
                rotate([0,0,bolt_ang]) translate([25,0,20]) cylinder(h=50, d=4.5, center=true);
            }
        }
    }
}

// Actuators placeholder (central X)
module central_actuators() {
    // Simplified X linear actuators
    for (a = [0:90:270]) {
        rotate([0,0,a]) translate([30,0,0]) cube([80,20,20], center=true);
    }
}

// Assembly
central_body();
central_actuators();

for (ang = [45, 135, 225, 315]) {
    arm(ang);
}

// Text label
translate([0,0,100]) linear_extrude(5) text("Morphing Star Quad-Glider", size=20, halign="center");
# SPDX-License-Identifier: Apache-2.0
# Copied verbatim from IHP-Open-PDK 5e6d592e ihp-sg13g2-util.tcl:688-787,
# Copyright IHP GmbH, Apache License 2.0; the reference for the native
# helper tests.
#---------------------
# Internal to Microns
#---------------------
proc magic::i2u { value } {
    set value [expr {((round([magic::cif scale output] * 10000)) / 10000.0) * $value}]
    return [format "%.3f" $value]
}

#---------------------
# Microns to Internal
#---------------------
proc magic::u2i {value} {
    return [expr {round($value / ((round([magic::cif scale output] * 10000)) / 10000.0))}]
}

#--------------------------------------------------------------------------
# Find the smallest integer value greater in magnitude than the given value
#--------------------------------------------------------------------------
proc magic::magceil {value} {
    if {$value < 0} {
	return [::tcl::mathfunc::floor $value]
    } else {
	return [::tcl::mathfunc::ceil $value]
    }
}

#--------------------------------------------------------------------------
# Find the largest integer value less in magnitude than the given value
#--------------------------------------------------------------------------
proc magic::magfloor {value} {
    if {$value < 0} {
	return [::tcl::mathfunc::ceil $value]
    } else {
	return [::tcl::mathfunc::floor $value]
    }
}

#---------------------
# Float to Spice 
#---------------------
proc magic::float2spice {value} { 
    if {$value >= 1.0e+6} { 
	set exponent 1e+6
	set unit "meg"
    } elseif {$value >= 1.0e+3} { 
	set exponent 1e+3
	set unit "k"
    } elseif { $value >= 1} { 
	set exponent 1
	set unit ""
    } elseif {$value >= 1.0e-3} { 
	set exponent 1e-3
	set unit "m"
    } elseif {$value >= 1.0e-6} { 
	set exponent 1e-6
	set unit "u"
    } elseif {$value >= 1.0e-9} { 
	set exponent 1e-9
	set unit "n"
    } elseif {$value >= 1.0e-12} { 
	set exponent 1e-12
	set unit "p"
    } elseif {$value >= 1.0e-15} { 
	set exponent 1e-15
	set unit "f"
    } else {
	set exponent 1e-18
	set unit "a"
    }
    set val [expr $value / $exponent]
    set val [expr int($val * 1000) / 1000.0]
    if {$val == 0} {set unit ""}
    return $val$unit
}

#---------------------
# Spice to Float
#---------------------
proc magic::spice2float {value {faultval 0.0}} { 
    # Remove trailing units, at least for some common combinations
    set value [string tolower $value]
    set value [string map {um u nm n uF n nF n pF p aF a} $value]
    set value [string map {meg "* 1.0e6" k "* 1.0e3" m "* 1.0e-3" u "* 1.0e-6" \
		 n "* 1.0 e-9" p "* 1.0e-12" f "* 1.0e-15" a "* 1.0e-15"} $value]
    if {[catch {set rval [expr $value]}]} {
	puts stderr "Value is not numeric!"
	set rval $faultval
    }
    return $rval
}

#---------------------
# Numeric Precision
#---------------------
proc magic::3digitpastdecimal {value} {
    set new [expr int([expr $value * 1000 + 0.5 ]) / 1000.0]
    return $new
}

#-------------------------------------------------------------------

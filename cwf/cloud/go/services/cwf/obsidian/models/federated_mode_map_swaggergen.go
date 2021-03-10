// Code generated by go-swagger; DO NOT EDIT.

package models

// This file was generated by the swagger tool.
// Editing this file might prove futile when you re-run the swagger generate command

import (
	"strconv"

	strfmt "github.com/go-openapi/strfmt"

	"github.com/go-openapi/errors"
	"github.com/go-openapi/swag"
)

// FederatedModeMap Mapping for PLMN, imsi ranges, vs its gateway mode
// swagger:model federatedModeMap
type FederatedModeMap struct {

	// If Disabled is True mapping will not be applied
	Disabled bool `json:"disabled,omitempty"`

	// mapping
	Mapping []*ModeMapItem `json:"mapping"`
}

// Validate validates this federated mode map
func (m *FederatedModeMap) Validate(formats strfmt.Registry) error {
	var res []error

	if err := m.validateMapping(formats); err != nil {
		res = append(res, err)
	}

	if len(res) > 0 {
		return errors.CompositeValidationError(res...)
	}
	return nil
}

func (m *FederatedModeMap) validateMapping(formats strfmt.Registry) error {

	if swag.IsZero(m.Mapping) { // not required
		return nil
	}

	for i := 0; i < len(m.Mapping); i++ {
		if swag.IsZero(m.Mapping[i]) { // not required
			continue
		}

		if m.Mapping[i] != nil {
			if err := m.Mapping[i].Validate(formats); err != nil {
				if ve, ok := err.(*errors.Validation); ok {
					return ve.ValidateName("mapping" + "." + strconv.Itoa(i))
				}
				return err
			}
		}

	}

	return nil
}

// MarshalBinary interface implementation
func (m *FederatedModeMap) MarshalBinary() ([]byte, error) {
	if m == nil {
		return nil, nil
	}
	return swag.WriteJSON(m)
}

// UnmarshalBinary interface implementation
func (m *FederatedModeMap) UnmarshalBinary(b []byte) error {
	var res FederatedModeMap
	if err := swag.ReadJSON(b, &res); err != nil {
		return err
	}
	*m = res
	return nil
}

// Code generated by go-swagger; DO NOT EDIT.

package wifi_networks

// This file was generated by the swagger tool.
// Editing this file might prove futile when you re-run the swagger generate command

import (
	"context"
	"net/http"
	"time"

	"github.com/go-openapi/errors"
	"github.com/go-openapi/runtime"
	cr "github.com/go-openapi/runtime/client"

	strfmt "github.com/go-openapi/strfmt"
)

// NewGetWifiNetworkIDDescriptionParams creates a new GetWifiNetworkIDDescriptionParams object
// with the default values initialized.
func NewGetWifiNetworkIDDescriptionParams() *GetWifiNetworkIDDescriptionParams {
	var ()
	return &GetWifiNetworkIDDescriptionParams{

		timeout: cr.DefaultTimeout,
	}
}

// NewGetWifiNetworkIDDescriptionParamsWithTimeout creates a new GetWifiNetworkIDDescriptionParams object
// with the default values initialized, and the ability to set a timeout on a request
func NewGetWifiNetworkIDDescriptionParamsWithTimeout(timeout time.Duration) *GetWifiNetworkIDDescriptionParams {
	var ()
	return &GetWifiNetworkIDDescriptionParams{

		timeout: timeout,
	}
}

// NewGetWifiNetworkIDDescriptionParamsWithContext creates a new GetWifiNetworkIDDescriptionParams object
// with the default values initialized, and the ability to set a context for a request
func NewGetWifiNetworkIDDescriptionParamsWithContext(ctx context.Context) *GetWifiNetworkIDDescriptionParams {
	var ()
	return &GetWifiNetworkIDDescriptionParams{

		Context: ctx,
	}
}

// NewGetWifiNetworkIDDescriptionParamsWithHTTPClient creates a new GetWifiNetworkIDDescriptionParams object
// with the default values initialized, and the ability to set a custom HTTPClient for a request
func NewGetWifiNetworkIDDescriptionParamsWithHTTPClient(client *http.Client) *GetWifiNetworkIDDescriptionParams {
	var ()
	return &GetWifiNetworkIDDescriptionParams{
		HTTPClient: client,
	}
}

/*GetWifiNetworkIDDescriptionParams contains all the parameters to send to the API endpoint
for the get wifi network ID description operation typically these are written to a http.Request
*/
type GetWifiNetworkIDDescriptionParams struct {

	/*NetworkID
	  Network ID

	*/
	NetworkID string

	timeout    time.Duration
	Context    context.Context
	HTTPClient *http.Client
}

// WithTimeout adds the timeout to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) WithTimeout(timeout time.Duration) *GetWifiNetworkIDDescriptionParams {
	o.SetTimeout(timeout)
	return o
}

// SetTimeout adds the timeout to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) SetTimeout(timeout time.Duration) {
	o.timeout = timeout
}

// WithContext adds the context to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) WithContext(ctx context.Context) *GetWifiNetworkIDDescriptionParams {
	o.SetContext(ctx)
	return o
}

// SetContext adds the context to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) SetContext(ctx context.Context) {
	o.Context = ctx
}

// WithHTTPClient adds the HTTPClient to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) WithHTTPClient(client *http.Client) *GetWifiNetworkIDDescriptionParams {
	o.SetHTTPClient(client)
	return o
}

// SetHTTPClient adds the HTTPClient to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) SetHTTPClient(client *http.Client) {
	o.HTTPClient = client
}

// WithNetworkID adds the networkID to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) WithNetworkID(networkID string) *GetWifiNetworkIDDescriptionParams {
	o.SetNetworkID(networkID)
	return o
}

// SetNetworkID adds the networkId to the get wifi network ID description params
func (o *GetWifiNetworkIDDescriptionParams) SetNetworkID(networkID string) {
	o.NetworkID = networkID
}

// WriteToRequest writes these params to a swagger request
func (o *GetWifiNetworkIDDescriptionParams) WriteToRequest(r runtime.ClientRequest, reg strfmt.Registry) error {

	if err := r.SetTimeout(o.timeout); err != nil {
		return err
	}
	var res []error

	// path param network_id
	if err := r.SetPathParam("network_id", o.NetworkID); err != nil {
		return err
	}

	if len(res) > 0 {
		return errors.CompositeValidationError(res...)
	}
	return nil
}

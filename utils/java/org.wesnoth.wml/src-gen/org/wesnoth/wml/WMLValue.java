/**
 * <copyright>
 * </copyright>
 *

 */
package org.wesnoth.wml;


/**
 * <!-- begin-user-doc -->
 * A representation of the model object '<em><b>WML Value</b></em>'.
 * <!-- end-user-doc -->
 *
 * <p>
 * The following features are supported:
 * <ul>
 *   <li>{@link org.wesnoth.wml.WMLValue#getValue <em>Value</em>}</li>
 * </ul>
 * </p>
 *
 * @see org.wesnoth.wml.WmlPackage#getWMLValue()
 * @model
 * @generated
 */
public interface WMLValue extends WMLKeyValue
{
  /**
   * Returns the value of the '<em><b>Value</b></em>' attribute.
   * <!-- begin-user-doc -->
   * <p>
   * If the meaning of the '<em>Value</em>' attribute isn't clear,
   * there really should be more of a description here...
   * </p>
   * <!-- end-user-doc -->
   * @return the value of the '<em>Value</em>' attribute.
   * @see #setValue(String)
   * @see org.wesnoth.wml.WmlPackage#getWMLValue_Value()
   * @model
   * @generated
   */
  String getValue();

  /**
   * Sets the value of the '{@link org.wesnoth.wml.WMLValue#getValue <em>Value</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @param value the new value of the '<em>Value</em>' attribute.
   * @see #getValue()
   * @generated
   */
  void setValue(String value);

} // WMLValue

/**
 * <copyright>
 * </copyright>
 *

 */
package org.wesnoth.wML;

import org.eclipse.emf.common.util.EList;

import org.eclipse.emf.ecore.EObject;

/**
 * <!-- begin-user-doc -->
 * A representation of the model object '<em><b>Macro</b></em>'.
 * <!-- end-user-doc -->
 *
 * <p>
 * The following features are supported:
 * <ul>
 *   <li>{@link org.wesnoth.wML.WMLMacro#getMacroName <em>Macro Name</em>}</li>
 *   <li>{@link org.wesnoth.wML.WMLMacro#getTagcontent <em>Tagcontent</em>}</li>
 * </ul>
 * </p>
 *
 * @see org.wesnoth.wML.WMLPackage#getWMLMacro()
 * @model
 * @generated
 */
public interface WMLMacro extends EObject
{
  /**
   * Returns the value of the '<em><b>Macro Name</b></em>' attribute.
   * <!-- begin-user-doc -->
   * <p>
   * If the meaning of the '<em>Macro Name</em>' attribute isn't clear,
   * there really should be more of a description here...
   * </p>
   * <!-- end-user-doc -->
   * @return the value of the '<em>Macro Name</em>' attribute.
   * @see #setMacroName(String)
   * @see org.wesnoth.wML.WMLPackage#getWMLMacro_MacroName()
   * @model
   * @generated
   */
  String getMacroName();

  /**
   * Sets the value of the '{@link org.wesnoth.wML.WMLMacro#getMacroName <em>Macro Name</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @param value the new value of the '<em>Macro Name</em>' attribute.
   * @see #getMacroName()
   * @generated
   */
  void setMacroName(String value);

  /**
   * Returns the value of the '<em><b>Tagcontent</b></em>' attribute list.
   * The list contents are of type {@link java.lang.String}.
   * <!-- begin-user-doc -->
   * <p>
   * If the meaning of the '<em>Tagcontent</em>' attribute list isn't clear,
   * there really should be more of a description here...
   * </p>
   * <!-- end-user-doc -->
   * @return the value of the '<em>Tagcontent</em>' attribute list.
   * @see org.wesnoth.wML.WMLPackage#getWMLMacro_Tagcontent()
   * @model unique="false"
   * @generated
   */
  EList<String> getTagcontent();

} // WMLMacro

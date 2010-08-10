/**
 * <copyright>
 * </copyright>
 *

 */
package org.wesnoth.wML;

import org.eclipse.emf.ecore.EFactory;

/**
 * <!-- begin-user-doc -->
 * The <b>Factory</b> for the model.
 * It provides a create method for each non-abstract class of the model.
 * <!-- end-user-doc -->
 * @see org.wesnoth.wML.WMLPackage
 * @generated
 */
public interface WMLFactory extends EFactory
{
  /**
   * The singleton instance of the factory.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  WMLFactory eINSTANCE = org.wesnoth.wML.impl.WMLFactoryImpl.init();

  /**
   * Returns a new object of class '<em>Root</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Root</em>'.
   * @generated
   */
  WMLRoot createWMLRoot();

  /**
   * Returns a new object of class '<em>Tag</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Tag</em>'.
   * @generated
   */
  WMLTag createWMLTag();

  /**
   * Returns a new object of class '<em>Abstract Macro Call</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Abstract Macro Call</em>'.
   * @generated
   */
  WMLAbstractMacroCall createWMLAbstractMacroCall();

  /**
   * Returns a new object of class '<em>Macro Include</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Macro Include</em>'.
   * @generated
   */
  WMLMacroInclude createWMLMacroInclude();

  /**
   * Returns a new object of class '<em>Macro Call</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Macro Call</em>'.
   * @generated
   */
  WMLMacroCall createWMLMacroCall();

  /**
   * Returns a new object of class '<em>Macro Define</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Macro Define</em>'.
   * @generated
   */
  WMLMacroDefine createWMLMacroDefine();

  /**
   * Returns a new object of class '<em>Textdomain</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Textdomain</em>'.
   * @generated
   */
  WMLTextdomain createWMLTextdomain();

  /**
   * Returns a new object of class '<em>Key</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Key</em>'.
   * @generated
   */
  WMLKey createWMLKey();

  /**
   * Returns a new object of class '<em>Key Extra Args</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Key Extra Args</em>'.
   * @generated
   */
  WMLKeyExtraArgs createWMLKeyExtraArgs();

  /**
   * Returns a new object of class '<em>Key Value</em>'.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return a new object of class '<em>Key Value</em>'.
   * @generated
   */
  WMLKeyValue createWMLKeyValue();

  /**
   * Returns the package supported by this factory.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @return the package supported by this factory.
   * @generated
   */
  WMLPackage getWMLPackage();

} //WMLFactory

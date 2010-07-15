/*******************************************************************************
 * Copyright (c) 2010 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package wesnoth_eclipse_plugin.wizards.generator;

public class TagKey
{
	public String	Name;
	/**
	 * Cardinality can be:
	 * 1 = required
	 * ? = optional
	 * * = repeated
	 * - = forbidden
	 */
	public char		Cardinality;
	public String	ValueType;
	public boolean	IsEnum;
	public boolean	IsTranslatable;

	public TagKey(String name, char cardinality, String valueType, boolean trans) {
		Name = name;
		Cardinality = cardinality;

		if (valueType == null)
		{
			return;
		}
		IsEnum = valueType.substring(1, valueType.indexOf(" ")).equals("enum");
		ValueType = valueType.substring(valueType.indexOf(" ") + 1, valueType.length() - 1); // remove the " "
		IsTranslatable = trans;
	}
}

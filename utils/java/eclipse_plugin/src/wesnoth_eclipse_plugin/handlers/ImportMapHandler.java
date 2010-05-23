package wesnoth_eclipse_plugin.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFolder;
import org.eclipse.swt.widgets.MessageBox;

import wesnoth_eclipse_plugin.Activator;
import wesnoth_eclipse_plugin.globalactions.MapActions;
import wesnoth_eclipse_plugin.utils.WorkspaceUtils;

public class ImportMapHandler extends AbstractHandler
{
	@Override
	public Object execute(ExecutionEvent event) throws ExecutionException
	{
		MapActions.importMap();
		return null;
	}

	@Override
	public boolean isHandled()
	{
		IFolder selectedFolder = WorkspaceUtils.getSelectedFolder(WorkspaceUtils.getWorkbenchWindow());

		if (!selectedFolder.getName().equals("maps"))
		{
			MessageBox box = new MessageBox(Activator.getShell());
			box.setMessage("You need to select a \"maps\" folder before importing anything");
			box.open();
		}

		return (selectedFolder != null && selectedFolder.getName().equals("maps"));
	}
}
